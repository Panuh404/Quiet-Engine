#include "Quiet/Editor/EditorController.h"

#include <memory>
#include <cmath>
#include <limits>
#include <utility>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/ext/matrix_projection.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "imgui.h"

#include "Quiet/Input/Input.h"
#include "Quiet/Math/Geometry.h"
#include "Quiet/Core/Log.h"
#include "Quiet/Project/ProjectContext.h"
#include "Renderer/Vulkan/Mesh/StaticMesh.h"
#include "Renderer/Vulkan/VulkanRenderer.h"
#include "Scene/Components/ScriptComponent.h"
#include "Scene/Actor.h"
#include "Scene/Camera/FreeCamera.h"
#include "Scene/Scene.h"
#include "Scripting/ScriptMetadata.h"

namespace Quiet::Editor
{
    void EditorController::ResetSelection()
    {
        m_State.Selection.SelectedActorID.reset();
        m_State.Selection.HoveredActorID.reset();
    }

    void EditorController::ResetSceneHistory(const Scene::Scene& scene)
    {
        m_History.Reset(scene, m_State);
        m_ScriptSourceWriteTimes.clear();
        m_State.Scripting.SourceErrors.clear();
    }

    void EditorController::MarkSceneSaved(const Scene::Scene& scene)
    {
        m_History.MarkSaved(scene, m_State);
    }

    void EditorController::BeginSceneMutation(const Scene::Scene& scene)
    {
        m_History.BeginMutation(scene, m_State);
    }

    bool EditorController::EndSceneMutation(const Scene::Scene& scene)
    {
        return m_History.EndMutation(scene, m_State);
    }

    bool EditorController::Undo(Scene::Scene& scene)
    {
        return m_History.Undo(scene, m_State);
    }

    bool EditorController::Redo(Scene::Scene& scene)
    {
        return m_History.Redo(scene, m_State);
    }

    void EditorController::SynchronizeScriptSources(Scene::Scene& scene, const Project::ProjectContext& projectContext)
    {
        if (m_State.Scene.Mode != SceneMode::Edit) return;

        // Actors are still grouped separately because exposed-property
        // reconciliation only applies to ScriptComponents currently
        // present in the Edit Scene.
        std::unordered_map<std::string, std::vector<Core::UUID>> actorsByScript;

        for (const Scene::Actor& actor : scene.GetActors())
        {
            const auto* script = actor.TryGetComponent<Scene::ScriptComponent>();
            if (!script || script->ScriptAssetID.empty()) continue;

            actorsByScript[script->ScriptAssetID].push_back(actor.GetID());
        }

        // Diagnostics, however, belong to Script assets themselves,
        // not only to scripts that happen to be assigned to an Actor.
        const std::vector<std::string> scriptAssetIDs = projectContext.EnumerateScriptAssetIDs();
        const std::unordered_set<std::string> registeredScripts(scriptAssetIDs.begin(), scriptAssetIDs.end());

        for (auto iterator = m_ScriptSourceWriteTimes.begin(); iterator != m_ScriptSourceWriteTimes.end();)
        {
            if (!registeredScripts.contains(iterator->first))
            {
                iterator = m_ScriptSourceWriteTimes.erase(iterator);
                continue;
            }
            ++iterator;
        }

        for (auto iterator = m_State.Scripting.SourceErrors.begin(); iterator != m_State.Scripting.SourceErrors.end();)
        {
            if (!registeredScripts.contains(iterator->first))
            {
                iterator = m_State.Scripting.SourceErrors.erase(iterator);
                continue;
            }
            ++iterator;
        }

        struct PendingRefresh
        {
            Core::UUID ActorID{};
            std::vector<Scene::ScriptProperty> Properties;
        };

        std::vector<PendingRefresh> pendingRefreshes;

        const auto setSourceError = [&](const std::string& scriptAssetID, const std::string& error)
        {
            auto& errors = m_State.Scripting.SourceErrors;

            const auto existing = errors.find(scriptAssetID);
            if (existing == errors.end() || existing->second != error)
            {
                QUIET_CORE_WARN("Script source '{}' has an editor metadata error: {}", scriptAssetID, error);
            }

            errors[scriptAssetID] =error;
        };

        for (const std::string& scriptAssetID : scriptAssetIDs)
        {
            std::filesystem::file_time_type writeTime{};
            std::string sourceError;

            if (!Scripting::GetScriptSourceWriteTime(projectContext, scriptAssetID, writeTime, sourceError))
            {
                m_ScriptSourceWriteTimes.erase(scriptAssetID);
                setSourceError(scriptAssetID, sourceError);
                continue;
            }

            const auto watched = m_ScriptSourceWriteTimes.find(scriptAssetID);
            const bool sourceChanged = watched == m_ScriptSourceWriteTimes.end() || watched->second != writeTime;
            if (!sourceChanged) continue;

            // This exact version is now considered attempted.
            // Broken source therefore isn't reparsed every frame.
            m_ScriptSourceWriteTimes[scriptAssetID] = writeTime;
            std::vector<Scene::ScriptProperty> discoveredProperties;

            std::string discoveryError;

            if (!Scripting::DiscoverScriptProperties(projectContext, scriptAssetID, discoveredProperties, discoveryError))
            {
                setSourceError(scriptAssetID, discoveryError);
                continue;
            }

            m_State.Scripting.SourceErrors.erase(scriptAssetID);
            const auto actorGroup = actorsByScript.find(scriptAssetID);
            if (actorGroup == actorsByScript.end()) continue;

            for (const Core::UUID actorID : actorGroup->second)
            {
                Scene::Actor* actor = scene.FindActorByID(actorID);
                if (!actor) continue;

                const auto* script = actor->TryGetComponent<Scene::ScriptComponent>();
                if (!script || script->ScriptAssetID != scriptAssetID) continue;

                std::vector<Scene::ScriptProperty> reconciled = discoveredProperties;
                for (Scene::ScriptProperty& discovered : reconciled)
                {
                    const auto existing = std::find_if(
                        script->Properties.begin(),
                        script->Properties.end(),
                        [&discovered](const Scene::ScriptProperty& property)
                        {
                            return property.Name == discovered.Name && property.Type == discovered.Type;
                        });

                    if (existing != script->Properties.end())
                    {
                        discovered = *existing;
                    }
                }

                if (script->Properties == reconciled) continue;
                pendingRefreshes.push_back(PendingRefresh{ actorID, std::move(reconciled) });
            }
        }

        if (pendingRefreshes.empty()) return;
        BeginSceneMutation(scene);

        for (PendingRefresh& refresh : pendingRefreshes)
        {
            Scene::Actor* actor = scene.FindActorByID(refresh.ActorID);
            if (!actor) continue;

            auto* script = actor->TryGetComponent<Scene::ScriptComponent>();
            if (!script) continue;
            script->Properties = std::move(refresh.Properties);
        }

        EndSceneMutation(scene);
    }

    void EditorController::HandleShortcuts(const Input::Input& input, bool mouseCaptureEnabled)
    {
        if (mouseCaptureEnabled) return;

        const ImGuiIO& io = ImGui::GetIO();
        if (io.WantCaptureKeyboard || io.WantTextInput) return;
        if (m_State.Scene.Mode != SceneMode::Edit) return;

        using Input::KeyCode;

        const bool ctrlDown =
            input.IsKeyDown(KeyCode::LeftControl) ||
            input.IsKeyDown(KeyCode::RightControl);

        if (ctrlDown) return;

        if (m_State.Viewport.NavigationCaptureRequested != ViewportNavigationMode::None ||
            m_State.Viewport.NavigationMode != ViewportNavigationMode::None)
        {
            return;
        }

        if (input.IsKeyPressed(KeyCode::T)) m_State.Gizmo.Operation = GizmoOperation::Translate;
        if (input.IsKeyPressed(KeyCode::R)) m_State.Gizmo.Operation = GizmoOperation::Rotate;
        if (input.IsKeyPressed(KeyCode::S)) m_State.Gizmo.Operation = GizmoOperation::Scale;
        if (input.IsKeyPressed(KeyCode::L))
        {
            if (m_State.Gizmo.Operation != GizmoOperation::Scale)
            {
                m_State.Gizmo.Mode =
                    (m_State.Gizmo.Mode == GizmoMode::World)
                        ? GizmoMode::Local
                        : GizmoMode::World;
            }
        }
        if (input.IsKeyPressed(KeyCode::G)) m_State.Gizmo.SnapEnabled = !m_State.Gizmo.SnapEnabled;
    }

    void EditorController::FocusSelectedActor(Scene::Scene& scene, Scene::FreeCamera& camera)
    {
        if (!m_State.Selection.SelectedActorID.has_value())
        {
            return;
        }

        Scene::Actor* actor = scene.FindActorByID(m_State.Selection.SelectedActorID.value());
        if (!actor)
        {
            m_State.Selection.SelectedActorID.reset();
            return;
        }

        const Scene::TransformComponent& transform = actor->GetComponent<Scene::TransformComponent>();
        glm::vec3 target = transform.WorldTransform.Position;

        float radius = 1.0f;

        const Scene::StaticMeshComponent* staticMesh = actor->TryGetComponent<Scene::StaticMeshComponent>();
        if (staticMesh && staticMesh->IsValid() && staticMesh->Visible)
        {
            const auto& mesh = staticMesh->Mesh;
            const auto localBounds = mesh->GetLocalBounds();

            const Math::AABB worldBounds =  Math::TransformAABB(transform.WorldTransform.ToMatrix(), localBounds.Min, localBounds.Max);
            target = (worldBounds.Min + worldBounds.Max) * 0.5f;
            radius = glm::max(0.5f, glm::length(worldBounds.Max - worldBounds.Min) * 0.5f);
        }

        const float fovRadians = glm::radians(camera.GetFovDegrees());
        float distance = radius / std::tan(fovRadians * 0.5f);
        distance = glm::max(distance * 1.5f, 1.5f);

        const glm::vec3 forward = camera.GetForwardVector();
        const glm::vec3 newPosition = target - forward * distance;

        camera.SetPosition(newPosition);
        QUIET_CORE_TRACE("Focused selected actor at ({:.2f}, {:.2f}, {:.2f}).", target.x, target.y, target.z);
    }
}
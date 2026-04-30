#include "Quiet/Editor/EditorCommands.h"

#include <utility>

#include "Quiet/Editor/EditorController.h"
#include "Quiet/Core/Log.h"
#include "Quiet/Project/AssetRecord.h"
#include "Quiet/Project/ProjectContext.h"
#include "Renderer/Vulkan/Mesh/MeshLibrary.h"
#include "Scene/Actor.h"
#include "Scene/Scene.h"

namespace
{
    template<typename Fn>
    bool ApplyImmediateMutation(
        Quiet::Editor::EditorController& editorController,
        Quiet::Scene::Scene& scene, Fn&& fn)
    {
        auto& editorState = editorController.GetState();
        if (editorState.Scene.Mode != Quiet::Editor::SceneMode::Edit)
            return false;

        editorController.BeginSceneMutation(scene);
        const bool changed = fn();
        const bool committed = editorController.EndSceneMutation(scene);
        return changed && committed;
    }

    const Quiet::Project::AssetRecord* FindMeshAssetRecord(
        const Quiet::Project::ProjectContext& projectContext,
        const std::string& meshResourceID)
    {
        const auto& registry = projectContext.GetAssetRegistry();
        const auto* record = Quiet::Project::AssetRegistryIO::FindByID(registry, meshResourceID);
        if (!record) return nullptr;

        if (record->Type != Quiet::Project::AssetType::Mesh) return nullptr;
        return record;
    }

    std::shared_ptr<Quiet::Renderer::Vulkan::StaticMesh> ResolveMeshResource(
        const Quiet::Project::ProjectContext& projectContext,
        const Quiet::Renderer::Vulkan::MeshLibrary& meshLibrary,
        const std::string& meshResourceID)
    {
        if (!FindMeshAssetRecord(projectContext, meshResourceID)) return nullptr;
        return meshLibrary.TryGet(meshResourceID);
    }
}

namespace Quiet::Editor
{
    EditorCommands::EditorCommands(EditorController& editorController)
        : m_EditorController(editorController) {}

    bool EditorCommands::CanEditScene() const
    {
        return m_EditorController.GetState().Scene.Mode == SceneMode::Edit;
    }

    void EditorCommands::BeginInteraction(const Scene::Scene& scene)
    {
        if (!CanEditScene()) return;
        m_EditorController.BeginSceneMutation(scene);
    }

    bool EditorCommands::EndInteraction(const Scene::Scene& scene)
    {
        if (!CanEditScene()) return false;
        return m_EditorController.EndSceneMutation(scene);
    }

    bool EditorCommands::CreateEmptyActor(Scene::Scene& scene, const std::string& name)
    {
        return ApplyImmediateMutation(m_EditorController, scene, [&]()
        {
            auto& actor = scene.CreateActor(name);
            m_EditorController.GetState().Selection.SelectedActorID = actor.ID;
            return true;
        });
    }

    bool EditorCommands::DuplicateSelectedActor(Scene::Scene& scene)
    {
        const auto selectedActorID = m_EditorController.GetState().Selection.SelectedActorID;
        if (!selectedActorID.has_value()) return false;
        return DuplicateActor(scene, selectedActorID.value());
    }

    bool EditorCommands::DeleteSelectedActor(Scene::Scene& scene)
    {
        const auto selectedActorID = m_EditorController.GetState().Selection.SelectedActorID;
        if (!selectedActorID.has_value()) return false;
        return DeleteActor(scene, selectedActorID.value());
    }

    bool EditorCommands::DuplicateActor(Scene::Scene& scene, Core::UUID actorID)
    {
        return ApplyImmediateMutation(m_EditorController, scene, [&]()
        {
            Scene::Actor* duplicate = scene.TryDuplicateActor(actorID);
            if (!duplicate) return false;
            m_EditorController.GetState().Selection.SelectedActorID = duplicate->ID;
            return true;
        });
    }

    bool EditorCommands::DeleteActor(Scene::Scene& scene, Core::UUID actorID)
    {
        return ApplyImmediateMutation(m_EditorController, scene, [&]()
        {
            const bool destroyed = scene.DestroyActorByID(actorID);
            if (!destroyed) return false;

            auto& editorState = m_EditorController.GetState();
            if (editorState.Selection.SelectedActorID.has_value() &&
                editorState.Selection.SelectedActorID.value() == actorID)
            {
                editorState.Selection.SelectedActorID.reset();
            }

            if (editorState.Selection.PilotedCameraActorID.has_value() &&
                editorState.Selection.PilotedCameraActorID.value() == actorID)
            {
                editorState.Selection.PilotedCameraActorID.reset();
            }

            return true;
        });
    }

    bool EditorCommands::RenameActor(Scene::Scene& scene,
                                     Core::UUID actorID,
                                     const std::string& newName)
    {
        Scene::Actor* actor = scene.FindActorByID(actorID);
        if (!actor) return false;

        actor->Name = newName;
        return true;
    }

    bool EditorCommands::SetActorPosition(Scene::Scene& scene,
                                          Core::UUID actorID,
                                          const glm::vec3& position)
    {
        Scene::Actor* actor = scene.FindActorByID(actorID);
        if (!actor) return false;

        actor->Transform.WorldTransform.Position = position;
        return true;
    }

    bool EditorCommands::SetActorRotation(Scene::Scene& scene,
                                          Core::UUID actorID,
                                          const glm::vec3& rotationDegrees)
    {
        Scene::Actor* actor = scene.FindActorByID(actorID);
        if (!actor) return false;

        actor->Transform.WorldTransform.RotationDegrees = rotationDegrees;
        return true;
    }

    bool EditorCommands::SetActorScale(Scene::Scene& scene,
                                       Core::UUID actorID,
                                       const glm::vec3& scale)
    {
        Scene::Actor* actor = scene.FindActorByID(actorID);
        if (!actor) return false;

        actor->Transform.WorldTransform.Scale = scale;
        return true;
    }

    bool EditorCommands::AddStaticMesh(Scene::Scene& scene,
                                       const Core::UUID actorID,
                                       const Project::ProjectContext& projectContext,
                                       const Renderer::Vulkan::MeshLibrary& meshLibrary)
    {
        const auto meshAssetIDs = projectContext.EnumerateMeshAssetIDs();
        if (meshAssetIDs.empty())
        {
            QUIET_CORE_WARN("No mesh assets are registered in the current project");
            return false;
        }

        const std::string& defaultMeshID = meshAssetIDs.front();
        const auto mesh = ResolveMeshResource(projectContext, meshLibrary, defaultMeshID);
        if (!mesh)
        {
            QUIET_CORE_WARN(
                "Default mesh asset '{}' is registered but not currently available in the mesh library.",
                defaultMeshID);
            return false;
        }

        return ApplyImmediateMutation(m_EditorController, scene, [&]() -> bool
        {
            auto* actor = scene.FindActorByID(actorID);
            if (!actor || actor->StaticMesh.has_value()) return false;

            actor->StaticMesh = Scene::StaticMeshComponent{};
            actor->StaticMesh->MeshResourceID = defaultMeshID;
            actor->StaticMesh->Mesh = mesh;
            actor->StaticMesh->Visible= true;
            return true;
        });
    }

    bool EditorCommands::RemoveStaticMesh(Scene::Scene& scene, Core::UUID actorID)
    {
        return ApplyImmediateMutation(m_EditorController, scene, [&]()
        {
            Scene::Actor* actor = scene.FindActorByID(actorID);
            if (!actor || !actor->StaticMesh.has_value()) return false;

            actor->StaticMesh.reset();
            return true;
        });
    }

    bool EditorCommands::SetStaticMeshResource(Scene::Scene& scene,
                                               Core::UUID actorID,
                                               const std::string& meshResourceID,
                                               const Project::ProjectContext& projectContext,
                                               const Renderer::Vulkan::MeshLibrary& meshLibrary)
    {
        const auto mesh = ResolveMeshResource(projectContext, meshLibrary, meshResourceID);
        if (!mesh)
        {
            QUIET_CORE_WARN(
                "Mesh asset '{}' is not registered in the project or is not currently loaded.",
                meshResourceID
            );
            return false;
        }

        return ApplyImmediateMutation(m_EditorController, scene, [&]() -> bool
        {
            auto* actor = scene.FindActorByID(actorID);
            if (!actor || !actor->StaticMesh.has_value()) return false;
            if (actor->StaticMesh->MeshResourceID == meshResourceID) return false;

            actor->StaticMesh->MeshResourceID = meshResourceID;
            actor->StaticMesh->Mesh = mesh;
            return true;
        });
    }

    bool EditorCommands::SetStaticMeshVisible(Scene::Scene& scene,
                                              Core::UUID actorID,
                                              bool visible)
    {
        return ApplyImmediateMutation(m_EditorController, scene, [&]()
        {
            Scene::Actor* actor = scene.FindActorByID(actorID);
            if (!actor || !actor->StaticMesh.has_value()) return false;

            actor->StaticMesh->Visible = visible;
            return true;
        });
    }

    bool EditorCommands::AddRotator(Scene::Scene& scene,
                                    Core::UUID actorID,
                                    float initialSpeed)
    {
        return ApplyImmediateMutation(m_EditorController, scene, [&]()
        {
            Scene::Actor* actor = scene.FindActorByID(actorID);
            if (!actor) return false;

            actor->Rotator = Scene::RotatorComponent{ initialSpeed };
            return true;
        });
    }

    bool EditorCommands::RemoveRotator(Scene::Scene& scene, Core::UUID actorID)
    {
        return ApplyImmediateMutation(m_EditorController, scene, [&]()
        {
            Scene::Actor* actor = scene.FindActorByID(actorID);
            if (!actor || !actor->Rotator.has_value()) return false;

            actor->Rotator.reset();
            return true;
        });
    }

    bool EditorCommands::SetRotatorSpeed(Scene::Scene& scene,
                                         Core::UUID actorID,
                                         float speed)
    {
        Scene::Actor* actor = scene.FindActorByID(actorID);
        if (!actor || !actor->Rotator.has_value()) return false;

        actor->Rotator->RotationSpeedDegreesPerSecond = speed;
        return true;
    }

    bool EditorCommands::AddCamera(Scene::Scene &scene, Core::UUID actorID)
    {
        return ApplyImmediateMutation(m_EditorController, scene, [&]()
        {
            Scene::Actor* actor = scene.FindActorByID(actorID);
            if (!actor || actor->Camera.has_value())
            {
                return false;
            }

            Scene::CameraComponent camera{};
            camera.Primary = (scene.FindPrimaryCameraActor() == nullptr);

            actor->Camera = camera;
            return true;
        });
    }

    bool EditorCommands::RemoveCamera(Scene::Scene& scene, Core::UUID actorID)
    {
        return ApplyImmediateMutation(m_EditorController, scene, [&]()
        {
            Scene::Actor* actor = scene.FindActorByID(actorID);
            if (!actor || !actor->Camera.has_value())
            {
                return false;
            }

            const bool wasPrimary = actor->Camera->Primary;
            actor->Camera.reset();

            auto& editorState = m_EditorController.GetState();
            if (editorState.Selection.PilotedCameraActorID.has_value() &&
                editorState.Selection.PilotedCameraActorID.value() == actorID)
            {
                editorState.Selection.PilotedCameraActorID.reset();
            }

            if (wasPrimary)
            {
                for (auto& other : scene.GetActors())
                {
                    if (other.Camera.has_value())
                    {
                        other.Camera->Primary = true;
                        break;
                    }
                }
            }
            return true;
        });
    }

    bool EditorCommands::SetCameraPrimary(Scene::Scene &scene, Core::UUID actorID, bool primary)
    {
        return ApplyImmediateMutation(m_EditorController, scene, [&]()
        {
            Scene::Actor* actor = scene.FindActorByID(actorID);
            if (!actor || !actor->Camera.has_value()) return false;

            if (primary)
            {
                for (auto& other : scene.GetActors())
                {
                    if (other.Camera.has_value())
                        other.Camera->Primary = false;
                }
            }
            actor->Camera->Primary = primary;
            return true;
        });
    }

    bool EditorCommands::SetCameraFieldOfView(Scene::Scene &scene, Core::UUID actorID, float fovDegrees)
    {
        Scene::Actor* actor = scene.FindActorByID(actorID);
        if (!actor || !actor->Camera.has_value()) return false;

        actor->Camera->FieldOfViewDegrees = fovDegrees;
        return true;
    }

    bool EditorCommands::SetCameraNearClip(Scene::Scene &scene, Core::UUID actorID, float nearClip)
    {
        Scene::Actor* actor = scene.FindActorByID(actorID);
        if (!actor || !actor->Camera.has_value()) return false;

        actor->Camera->NearClip = nearClip;
        return true;
    }

    bool EditorCommands::SetCameraFarClip(Scene::Scene &scene, Core::UUID actorID, float farClip)
    {
        Scene::Actor* actor = scene.FindActorByID(actorID);
        if (!actor || !actor->Camera.has_value()) return false;

        actor->Camera->FarClip = farClip;
        return true;
    }
}

#include "Quiet/Editor/EditorCommands.h"

#include <utility>

#include "Quiet/Editor/EditorController.h"
#include "Quiet/Core/Log.h"
#include "Renderer/Vulkan/MeshLibrary.h"
#include "Scene/Actor.h"
#include "Scene/Scene.h"

namespace
{
    template<typename Fn>
    bool ApplyImmediateMutation(Quiet::Editor::EditorController& editorController,
                                Quiet::Scene::Scene& scene,
                                Fn&& fn)
    {
        auto& editorState = editorController.GetState();
        if (editorState.CurrentSceneMode != Quiet::Editor::SceneMode::Edit)
            return false;

        editorController.BeginSceneMutation(scene);
        const bool changed = fn();
        const bool committed = editorController.EndSceneMutation(scene);
        return changed && committed;
    }
}

namespace Quiet::Editor
{
    EditorCommands::EditorCommands(EditorController& editorController)
        : m_EditorController(editorController) {}

    bool EditorCommands::CanEditScene() const
    {
        return m_EditorController.GetState().CurrentSceneMode == SceneMode::Edit;
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
            m_EditorController.GetState().SelectedActorID = actor.ID;
            return true;
        });
    }

    bool EditorCommands::DuplicateSelectedActor(Scene::Scene& scene)
    {
        const auto selectedActorID = m_EditorController.GetState().SelectedActorID;
        if (!selectedActorID.has_value()) return false;
        return DuplicateActor(scene, selectedActorID.value());
    }

    bool EditorCommands::DeleteSelectedActor(Scene::Scene& scene)
    {
        const auto selectedActorID = m_EditorController.GetState().SelectedActorID;
        if (!selectedActorID.has_value()) return false;
        return DeleteActor(scene, selectedActorID.value());
    }

    bool EditorCommands::DuplicateActor(Scene::Scene& scene, Core::UUID actorID)
    {
        return ApplyImmediateMutation(m_EditorController, scene, [&]()
        {
            Scene::Actor* duplicate = scene.TryDuplicateActor(actorID);
            if (!duplicate) return false;
            m_EditorController.GetState().SelectedActorID = duplicate->ID;
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
            if (editorState.SelectedActorID.has_value() &&
                editorState.SelectedActorID.value() == actorID)
            {
                editorState.SelectedActorID.reset();
            }

            if (editorState.PilotedCameraActorID.has_value() &&
                editorState.PilotedCameraActorID.value() == actorID)
            {
                editorState.PilotedCameraActorID.reset();
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
                                       Core::UUID actorID,
                                       const Renderer::Vulkan::MeshLibrary& meshLibrary)
    {
        return ApplyImmediateMutation(m_EditorController, scene, [&]()
        {
            Scene::Actor* actor = scene.FindActorByID(actorID);
            if (!actor) return false;

            const auto names = meshLibrary.GetNames();
            if (names.empty())
            {
                QUIET_CORE_WARN("Cannot add StaticMesh component. Mesh library is empty.");
                return false;
            }

            const std::string& meshID = names.front();
            if (!meshLibrary.Has(meshID)) return false;

            actor->StaticMesh = Scene::StaticMeshComponent{
                meshID,
                meshLibrary.Get(meshID),
                true
            };

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
                                               const Renderer::Vulkan::MeshLibrary& meshLibrary)
    {
        return ApplyImmediateMutation(m_EditorController, scene, [&]()
        {
            Scene::Actor* actor = scene.FindActorByID(actorID);
            if (!actor || !actor->StaticMesh.has_value()) return false;

            if (!meshLibrary.Has(meshResourceID))
            {
                QUIET_CORE_WARN("Mesh resource '{}' is not registered.", meshResourceID);
                return false;
            }

            actor->StaticMesh->MeshResourceID = meshResourceID;
            actor->StaticMesh->Mesh = meshLibrary.Get(meshResourceID);
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
            if (editorState.PilotedCameraActorID.has_value() &&
                editorState.PilotedCameraActorID.value() == actorID)
            {
                editorState.PilotedCameraActorID.reset();
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

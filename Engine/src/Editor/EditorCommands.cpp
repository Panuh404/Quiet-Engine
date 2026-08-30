#include "Quiet/Editor/EditorCommands.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "Quiet/Editor/EditorController.h"
#include "Quiet/Project/AssetRecord.h"
#include "Quiet/Project/ProjectContext.h"
#include "Quiet/Core/Log.h"
#include "Renderer/Vulkan/Mesh/MeshLibrary.h"
#include "Scene/Actor.h"
#include "Scene/Scene.h"

namespace
{
    template<typename Fn>
    bool ApplyImmediateMutation(Quiet::Editor::EditorController& editorController, Quiet::Scene::Scene& scene, Fn&& fn)
    {
        auto& editorState = editorController.GetState();
        if (editorState.Scene.Mode != Quiet::Editor::SceneMode::Edit)
            return false;

        editorController.BeginSceneMutation(scene);
        const bool changed = fn();
        const bool committed = editorController.EndSceneMutation(scene);
        return changed && committed;
    }

    const Quiet::Project::AssetRecord* FindMeshAssetRecord(const Quiet::Project::ProjectContext& projectContext, const std::string& meshResourceID)
    {
        const auto& registry = projectContext.GetAssetRegistry();
        const auto* record = Quiet::Project::AssetRegistryIO::FindByID(registry, meshResourceID);
        if (!record) return nullptr;

        if (record->Type != Quiet::Project::AssetType::Mesh) return nullptr;
        return record;
    }

    std::shared_ptr<Quiet::Renderer::Vulkan::StaticMesh> ResolveMeshResource(const Quiet::Project::ProjectContext& projectContext,
                                                                             const Quiet::Renderer::Vulkan::MeshLibrary& meshLibrary,
                                                                             const std::string& meshResourceID)
    {
        if (!FindMeshAssetRecord(projectContext, meshResourceID)) return nullptr;
        return meshLibrary.TryGet(meshResourceID);
    }

    std::vector<Quiet::Scene::MaterialComponent> CreateDefaultMaterialSlots(const std::shared_ptr<Quiet::Renderer::Vulkan::StaticMesh>& mesh)
    {
        std::vector<Quiet::Scene::MaterialComponent> slots;
        if (!mesh) return slots;

        const std::vector<Quiet::Renderer::Vulkan::StaticMeshMaterialSlot>& importedSlots = mesh->GetImportedMaterialSlots();

        if (!importedSlots.empty())
        {
            slots.reserve(importedSlots.size());
            for (const Quiet::Renderer::Vulkan::StaticMeshMaterialSlot& importedSlot : importedSlots)
            {
                Quiet::Scene::MaterialComponent material{};
                material.BaseColor = importedSlot.BaseColor;
                material.BaseColorTextureAssetID = importedSlot.BaseColorTextureAssetID;
                slots.push_back(std::move(material));
            }
            return slots;
        }

        std::uint32_t highestMaterialSlot = 0;
        bool hasMaterialSlot = false;

        for (const Quiet::Renderer::Vulkan::StaticSubmesh& submesh : mesh->GetSubmeshes())
        {
            highestMaterialSlot = std::max(highestMaterialSlot, submesh.MaterialSlot);
            hasMaterialSlot = true;
        }

        if (!hasMaterialSlot) return slots;

        slots.resize(static_cast<std::size_t>(highestMaterialSlot) + 1u);
        return slots;
    }
}

namespace Quiet::Editor
{
    EditorCommands::EditorCommands(EditorController& editorController) : m_EditorController(editorController) {}

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

#pragma region Actors

    bool EditorCommands::CreateEmptyActor(Scene::Scene& scene, const std::string& name)
    {
        return ApplyImmediateMutation(m_EditorController, scene, [&]()
        {
            auto& actor = scene.CreateActor(name);
            m_EditorController.GetState().Selection.SelectedActorID = actor.GetID();
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
            m_EditorController.GetState().Selection.SelectedActorID = duplicate->GetID();
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

    bool EditorCommands::RenameActor(Scene::Scene& scene, Core::UUID actorID, const std::string& newName)
    {
        if (!CanEditScene()) return false;

        Scene::Actor* actor = scene.FindActorByID(actorID);
        if (!actor || actor->GetName() == newName) return false;

        actor->GetName() = newName;
        return true;
    }

    bool EditorCommands::SetActorPosition(Scene::Scene& scene, Core::UUID actorID, const glm::vec3& position)
    {
        if (!CanEditScene() ||
            !std::isfinite(position.x) ||
            !std::isfinite(position.y) ||
            !std::isfinite(position.z))
        {
            return false;
        }

        Scene::Actor* actor = scene.FindActorByID(actorID);
        if (!actor)
            return false;

        actor->GetComponent<Scene::TransformComponent>().WorldTransform.Position = position;
        return true;
    }

    bool EditorCommands::SetActorRotation(Scene::Scene& scene, Core::UUID actorID, const glm::vec3& rotationDegrees)
    {
        if (!CanEditScene() ||
            !std::isfinite(rotationDegrees.x) ||
            !std::isfinite(rotationDegrees.y) ||
            !std::isfinite(rotationDegrees.z))
        {
            return false;
        }

        Scene::Actor* actor = scene.FindActorByID(actorID);
        if (!actor)
            return false;

        actor->GetComponent<Scene::TransformComponent>().WorldTransform.RotationDegrees = rotationDegrees;
        return true;
    }

    bool EditorCommands::SetActorScale(Scene::Scene& scene, Core::UUID actorID, const glm::vec3& scale)
    {
        if (!CanEditScene() ||
            !std::isfinite(scale.x) ||
            !std::isfinite(scale.y) ||
            !std::isfinite(scale.z))
        {
            return false;
        }

        Scene::Actor* actor = scene.FindActorByID(actorID);
        if (!actor)
            return false;

        actor->GetComponent<Scene::TransformComponent>().WorldTransform.Scale =
        {
            std::clamp(scale.x, 0.01f, 100.0f),
            std::clamp(scale.y, 0.01f, 100.0f),
            std::clamp(scale.z, 0.01f, 100.0f)
        };
        return true;
    }

    bool EditorCommands::ResetActorTransform(Scene::Scene &scene, Core::UUID actorID)
    {
        return ApplyImmediateMutation(m_EditorController, scene, [&]()
            {
                Scene::Actor* actor = scene.FindActorByID(actorID);
                if (!actor)
                    return false;

                actor->GetComponent<Scene::TransformComponent>() = Scene::TransformComponent{};
                return true;
            });
    }

    bool EditorCommands::PasteActorTransformValues(Scene::Scene& scene, Core::UUID actorID, const Scene::TransformComponent& transform)
    {
        const Scene::Transform& source = transform.WorldTransform;

        if (!std::isfinite(source.Position.x) ||
            !std::isfinite(source.Position.y) ||
            !std::isfinite(source.Position.z) ||
            !std::isfinite(source.RotationDegrees.x) ||
            !std::isfinite(source.RotationDegrees.y) ||
            !std::isfinite(source.RotationDegrees.z) ||
            !std::isfinite(source.Scale.x) ||
            !std::isfinite(source.Scale.y) ||
            !std::isfinite(source.Scale.z))
        {
            return false;
        }

        Scene::TransformComponent validatedTransform = transform;

        validatedTransform.WorldTransform.Scale =
        {
            std::clamp(source.Scale.x, 0.01f, 100.0f),
            std::clamp(source.Scale.y, 0.01f, 100.0f),
            std::clamp(source.Scale.z, 0.01f, 100.0f)
        };

        return ApplyImmediateMutation(
            m_EditorController,
            scene,
            [&]()
            {
                Scene::Actor* actor = scene.FindActorByID(actorID);
                if (!actor) return false;

                actor->GetComponent<Scene::TransformComponent>() = validatedTransform;
                return true;
            }
        );
    }

#pragma endregion

#pragma region Folders

    bool EditorCommands::CreateHierarchyFolder(Scene::Scene &scene, const std::string &name)
    {
        return ApplyImmediateMutation(m_EditorController, scene, [&]() -> bool
        {
            const Core::UUID folderID = scene.CreateHierarchyFolder(name.empty() ? "Folder" : name);
            return folderID.IsValid();
        });
    }

    bool EditorCommands::RenameHierarchyFolder(Scene::Scene &scene, Core::UUID folderID, const std::string &newName)
    {
        return ApplyImmediateMutation(m_EditorController, scene, [&]() -> bool
        {
            return scene.RenameHierarchyFolder(folderID, newName.empty() ? "Folder" : newName);
        });
    }

    bool EditorCommands::DeleteEmptyHierarchyFolder(Scene::Scene &scene, Core::UUID folderID)
    {
        return ApplyImmediateMutation(m_EditorController, scene, [&]() -> bool
        {
            return scene.DeleteEmptyHierarchyFolder(folderID);
        });
    }

    bool EditorCommands::MoveActorToHierarchyFolder(Scene::Scene &scene, Core::UUID actorID, Core::UUID folderID)
    {
        return ApplyImmediateMutation(m_EditorController, scene, [&]() -> bool
        {
            return scene.MoveActorToHierarchyFolder(actorID, folderID);
        });
    }

    bool EditorCommands::MoveActorToHierarchyRoot(Scene::Scene &scene, Core::UUID actorID)
    {
        return ApplyImmediateMutation(m_EditorController, scene, [&]() -> bool
        {
            return scene.MoveActorToHierarchyRoot(actorID);
        });
    }

    bool EditorCommands::MoveHierarchyRootNode(Scene::Scene &scene, Scene::SceneHierarchyNodeType nodeType, Core::UUID nodeID, std::size_t destinationIndex)
    {
        return ApplyImmediateMutation(m_EditorController, scene, [&]() -> bool
        {
            return scene.MoveHierarchyRootNode(nodeType, nodeID, destinationIndex);
        });
    }

#pragma endregion

#pragma region Static Meshes

    bool EditorCommands::CreateStaticMeshActor(Scene::Scene& scene, const std::string& actorName, const std::string& meshResourceID,
                                               const Project::ProjectContext& projectContext, const Renderer::Vulkan::MeshLibrary& meshLibrary,
                                               const glm::vec3& position)
    {
        const auto mesh = ResolveMeshResource(projectContext, meshLibrary, meshResourceID);
        if (!mesh)
        {
            QUIET_CORE_WARN("Cannot create mesh actor because mesh asset '{}' is not registered or loaded.", meshResourceID);
            return false;
        }
        return ApplyImmediateMutation(m_EditorController, scene, [&]() -> bool
        {
            Scene::Actor& actor = scene.CreateActor(actorName.empty() ? meshResourceID : actorName);
            actor.GetComponent<Scene::TransformComponent>().WorldTransform.Position = position;

            Scene::StaticMeshComponent& staticMesh = actor.AddComponent<Scene::StaticMeshComponent>();
            staticMesh.MeshResourceID = meshResourceID;
            staticMesh.Mesh = mesh;
            staticMesh.MaterialSlots = CreateDefaultMaterialSlots(mesh);
            staticMesh.Visible = true;

            actor.AddComponent<Scene::MaterialComponent>();

            m_EditorController.GetState().Selection.SelectedActorID = actor.GetID();
            return true;
        });
    }

    bool EditorCommands::AddStaticMesh(Scene::Scene& scene, const Core::UUID actorID, const Project::ProjectContext& projectContext, const Renderer::Vulkan::MeshLibrary& meshLibrary)
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
            QUIET_CORE_WARN("Default mesh asset '{}' is registered but not currently available in the mesh library.", defaultMeshID);
            return false;
        }

        return ApplyImmediateMutation(m_EditorController, scene, [&]() -> bool
        {
            Scene::Actor* actor = scene.FindActorByID(actorID);
            if (!actor || actor->HasComponent<Scene::StaticMeshComponent>()) return false;

            Scene::StaticMeshComponent& staticMesh = actor->AddComponent<Scene::StaticMeshComponent>();
            staticMesh.MeshResourceID = defaultMeshID;
            staticMesh.Mesh = mesh;
            staticMesh.MaterialSlots = CreateDefaultMaterialSlots(mesh);
            staticMesh.Visible = true;

            if (!actor->HasComponent<Scene::MaterialComponent>())
            {
                actor->AddComponent<Scene::MaterialComponent>();
            }
            return true;
        });
    }

    bool EditorCommands::RemoveStaticMesh(Scene::Scene& scene, Core::UUID actorID)
    {
        return ApplyImmediateMutation(m_EditorController, scene, [&]()
        {
            Scene::Actor* actor = scene.FindActorByID(actorID);
            if (!actor || !actor->HasComponent<Scene::StaticMeshComponent>()) return false;
            actor->RemoveComponent<Scene::StaticMeshComponent>();
            actor->RemoveComponent<Scene::MaterialComponent>();
            return true;
        });
    }

    bool EditorCommands::PasteStaticMeshValues(Scene::Scene &scene, Core::UUID actorID, const std::string &meshResourceID, bool visible, const Project::ProjectContext &projectContext, const Renderer::Vulkan::MeshLibrary &meshLibrary)
    {
        const auto mesh = ResolveMeshResource(projectContext, meshLibrary, meshResourceID);
        if (!mesh)
        {
            QUIET_CORE_WARN("Cannot paste Static Mesh values because mesh asset '{}' is not registered or loaded.", meshResourceID);
            return false;
        }

        return ApplyImmediateMutation(m_EditorController, scene, [&]()
        {
            Scene::Actor* actor = scene.FindActorByID(actorID);
            if (!actor) return false;

            Scene::StaticMeshComponent* staticMesh = actor->TryGetComponent<Scene::StaticMeshComponent>();
            if (!staticMesh) return false;

            const bool resourceChanged = staticMesh->MeshResourceID != meshResourceID;
            staticMesh->MeshResourceID = meshResourceID;
            staticMesh->Mesh = mesh;
            staticMesh->Visible = visible;

            if (resourceChanged)
            {
                staticMesh->MaterialSlots = CreateDefaultMaterialSlots(mesh);
            }

            return true;
        });
    }

    bool EditorCommands::SetStaticMeshResource(Scene::Scene& scene, Core::UUID actorID, const std::string& meshResourceID, const Project::ProjectContext& projectContext, const Renderer::Vulkan::MeshLibrary& meshLibrary)
    {
        const auto mesh = ResolveMeshResource(projectContext, meshLibrary, meshResourceID);
        if (!mesh)
        {
            QUIET_CORE_WARN("Mesh asset '{}' is not registered in the project or is not currently loaded.", meshResourceID);
            return false;
        }

        return ApplyImmediateMutation(m_EditorController, scene, [&]() -> bool
        {
            Scene::Actor* actor = scene.FindActorByID(actorID);
            if (!actor) return false;

            Scene::StaticMeshComponent* staticMesh = actor->TryGetComponent<Scene::StaticMeshComponent>();
            if (!staticMesh) return false;
            if (staticMesh->MeshResourceID == meshResourceID) return false;

            staticMesh->MeshResourceID = meshResourceID;
            staticMesh->Mesh = mesh;
            staticMesh->MaterialSlots = CreateDefaultMaterialSlots(mesh);

            return true;
        });
    }

    bool EditorCommands::SetStaticMeshVisible(Scene::Scene& scene, Core::UUID actorID, bool visible)
    {
        return ApplyImmediateMutation(m_EditorController, scene, [&]()
        {
            Scene::Actor* actor = scene.FindActorByID(actorID);
            if (!actor) return false;

            Scene::StaticMeshComponent* staticMesh = actor->TryGetComponent<Scene::StaticMeshComponent>();
            if (!staticMesh) return false;
            staticMesh->Visible = visible;

            return true;
        });
    }

#pragma endregion

#pragma region Material

    bool EditorCommands::AddMaterial(Scene::Scene& scene, Core::UUID actorID, const glm::vec4& baseColor)
    {
        if (!std::isfinite(baseColor.x) ||
            !std::isfinite(baseColor.y) ||
            !std::isfinite(baseColor.z) ||
            !std::isfinite(baseColor.w))
        {
            return false;
        }

        const glm::vec4 validatedColor
        {
            std::clamp(baseColor.x, 0.0f, 1.0f),
            std::clamp(baseColor.y, 0.0f, 1.0f),
            std::clamp(baseColor.z, 0.0f, 1.0f),
            std::clamp(baseColor.w, 0.0f, 1.0f)
        };

        return ApplyImmediateMutation(m_EditorController, scene, [&]()
        {
            Scene::Actor* actor = scene.FindActorByID(actorID);
            if (!actor || actor->HasComponent<Scene::MaterialComponent>()) return false;

            Scene::MaterialComponent& material = actor->AddComponent<Scene::MaterialComponent>();
            material.BaseColor = validatedColor;
            return true;
        });
    }

    bool EditorCommands::RemoveMaterial(Scene::Scene &scene, Core::UUID actorID)
    {
        return ApplyImmediateMutation(m_EditorController, scene, [&]()
        {
            Scene::Actor* actor = scene.FindActorByID(actorID);
            if (!actor || !actor->HasComponent<Scene::MaterialComponent>()) return false;

            return actor->RemoveComponent<Scene::MaterialComponent>();
        });
    }

    bool EditorCommands::ResetMaterial(Scene::Scene &scene, Core::UUID actorID)
    {
        return ApplyImmediateMutation(m_EditorController, scene, [&]()
        {
            Scene::Actor* actor = scene.FindActorByID(actorID);
            if (!actor) return false;

            Scene::StaticMeshComponent* staticMesh = actor->TryGetComponent<Scene::StaticMeshComponent>();
            Scene::MaterialComponent* material = actor->TryGetComponent<Scene::MaterialComponent>();
            if (!staticMesh || !staticMesh->Mesh || !material) return false;

            *material = Scene::MaterialComponent{};
            staticMesh->MaterialSlots = CreateDefaultMaterialSlots(staticMesh->Mesh);
            return true;
        });
    }

    bool EditorCommands::PasteMaterialValues(Scene::Scene& scene,
                                             Core::UUID actorID,
                                             const Scene::MaterialComponent& material,
                                             const std::vector<Scene::MaterialComponent>& materialSlots,
                                             const Project::ProjectContext& projectContext)
    {
        const auto& assetRegistry = projectContext.GetAssetRegistry();

        const auto isRegisteredTextureAsset =
            [&](const std::string& textureAssetID)
            {
                if (textureAssetID.empty()) return true;

                const Project::AssetRecord* record = Project::AssetRegistryIO::FindByID(assetRegistry, textureAssetID);
                return record && record->Type == Project::AssetType::Texture;
            };

        if (!isRegisteredTextureAsset(material.BaseColorTextureAssetID))
        {
            return false;
        }

        if (!std::isfinite(material.BaseColor.x) ||
            !std::isfinite(material.BaseColor.y) ||
            !std::isfinite(material.BaseColor.z) ||
            !std::isfinite(material.BaseColor.w))
        {
            return false;
        }

        Scene::MaterialComponent validatedMaterial = material;
        validatedMaterial.BaseColor =
        {
            std::clamp(material.BaseColor.x, 0.0f, 1.0f),
            std::clamp(material.BaseColor.y, 0.0f, 1.0f),
            std::clamp(material.BaseColor.z, 0.0f, 1.0f),
            std::clamp(material.BaseColor.w, 0.0f, 1.0f)
        };

        std::vector<Scene::MaterialComponent> validatedMaterialSlots = materialSlots;
        for (Scene::MaterialComponent& materialSlot : validatedMaterialSlots)
        {
            if (!isRegisteredTextureAsset(materialSlot.BaseColorTextureAssetID))
            {
                return false;
            }

            const glm::vec4 sourceColor = materialSlot.BaseColor;

            if (!std::isfinite(sourceColor.x) ||
                !std::isfinite(sourceColor.y) ||
                !std::isfinite(sourceColor.z) ||
                !std::isfinite(sourceColor.w))
            {
                return false;
            }

            materialSlot.BaseColor =
            {
                std::clamp(sourceColor.x, 0.0f, 1.0f),
                std::clamp(sourceColor.y, 0.0f, 1.0f),
                std::clamp(sourceColor.z, 0.0f, 1.0f),
                std::clamp(sourceColor.w, 0.0f, 1.0f)
            };
        }

        return ApplyImmediateMutation(m_EditorController, scene, [&]()
        {
            Scene::Actor* actor = scene.FindActorByID(actorID);
            if (!actor) return false;

            Scene::StaticMeshComponent* staticMesh = actor->TryGetComponent<Scene::StaticMeshComponent>();
            Scene::MaterialComponent* material = actor->TryGetComponent<Scene::MaterialComponent>();
            if (!staticMesh || !material) return false;

            if (staticMesh->MaterialSlots.size() != validatedMaterialSlots.size())
            {
                return false;
            }

            *material = validatedMaterial;
            staticMesh->MaterialSlots = validatedMaterialSlots;
            return true;
        });
    }

    bool EditorCommands::SetMaterialBaseColor(Scene::Scene& scene, Core::UUID actorID, const glm::vec4& baseColor)
    {
        if (!CanEditScene() ||
            !std::isfinite(baseColor.x) ||
            !std::isfinite(baseColor.y) ||
            !std::isfinite(baseColor.z) ||
            !std::isfinite(baseColor.w))
        {
            return false;
        }

        Scene::Actor* actor = scene.FindActorByID(actorID);
        if (!actor) return false;

        Scene::MaterialComponent* material = actor->TryGetComponent<Scene::MaterialComponent>();

        if (!material) return false;
        material->BaseColor =
        {
            std::clamp(baseColor.x, 0.0f, 1.0f),
            std::clamp(baseColor.y, 0.0f, 1.0f),
            std::clamp(baseColor.z, 0.0f, 1.0f),
            std::clamp(baseColor.w, 0.0f, 1.0f)
        };

        return true;
    }

    bool EditorCommands::SetMaterialBaseColorTextureAsset(Scene::Scene &scene, Core::UUID actorID, const std::string &textureAssetID)
    {
        return ApplyImmediateMutation(m_EditorController, scene, [&]()
        {
            Scene::Actor* actor = scene.FindActorByID(actorID);
            if (!actor) return false;

            Scene::MaterialComponent* material = actor->TryGetComponent<Scene::MaterialComponent>();
            if (!material) return false;
            material->BaseColorTextureAssetID = textureAssetID;

            return true;
        });
    }

    bool EditorCommands::SetStaticMeshMaterialSlotBaseColor(Scene::Scene& scene, Core::UUID actorID, std::uint32_t slotIndex, const glm::vec4& baseColor)
    {
        if (!CanEditScene() ||
            !std::isfinite(baseColor.x) ||
            !std::isfinite(baseColor.y) ||
            !std::isfinite(baseColor.z) ||
            !std::isfinite(baseColor.w))
        {
            return false;
        }

        Scene::Actor* actor = scene.FindActorByID(actorID);
        if (!actor) return false;

        Scene::StaticMeshComponent* staticMesh = actor->TryGetComponent<Scene::StaticMeshComponent>();
        if (!staticMesh) return false;

        Scene::MaterialComponent* materialSlot = staticMesh->TryGetMaterialSlot(slotIndex);
        if (!materialSlot) return false;
        materialSlot->BaseColor =
        {
            std::clamp(baseColor.x, 0.0f, 1.0f),
            std::clamp(baseColor.y, 0.0f, 1.0f),
            std::clamp(baseColor.z, 0.0f, 1.0f),
            std::clamp(baseColor.w, 0.0f, 1.0f)
        };

        return true;
    }

    bool EditorCommands::SetStaticMeshMaterialSlotBaseColorTextureAsset(Scene::Scene &scene, Core::UUID actorID, std::uint32_t slotIndex, const std::string &textureAssetID)
    {
        return ApplyImmediateMutation(m_EditorController, scene, [&]()
        {
            Scene::Actor* actor = scene.FindActorByID(actorID);
            if (!actor) return false;

            Scene::StaticMeshComponent* staticMesh = actor->TryGetComponent<Scene::StaticMeshComponent>();
            if (!staticMesh) return false;

            Scene::MaterialComponent* materialSlot = staticMesh->TryGetMaterialSlot(slotIndex);
            if (!materialSlot) return false;
            materialSlot->BaseColorTextureAssetID = textureAssetID;

            return true;
        });
    }

#pragma endregion

#pragma region Directional Lights

    bool EditorCommands::CreateDirectionalLightActor(Scene::Scene& scene, const std::string& actorName)
    {
        return ApplyImmediateMutation(m_EditorController, scene, [&]() -> bool
        {
            Scene::Actor& actor = scene.CreateActor(actorName.empty() ? "Directional Light" : actorName);
            actor.AddComponent<Scene::DirectionalLightComponent>();
            actor.GetComponent<Scene::TransformComponent>().WorldTransform.RotationDegrees = { 45.0f, -35.0f, 0.0f };

            m_EditorController.GetState().Selection.SelectedActorID = actor.GetID();
            return true;
        });
    }

    bool EditorCommands::AddDirectionalLight(Scene::Scene& scene, Core::UUID actorID)
    {
        return ApplyImmediateMutation(m_EditorController, scene, [&]()
        {
            Scene::Actor* actor = scene.FindActorByID(actorID);
            if (!actor || actor->HasComponent<Scene::DirectionalLightComponent>()) return false;
            actor->AddComponent<Scene::DirectionalLightComponent>();
            return true;
        });
    }

    bool EditorCommands::RemoveDirectionalLight(Scene::Scene& scene, Core::UUID actorID)
    {
        return ApplyImmediateMutation(m_EditorController, scene, [&]()
        {
            Scene::Actor* actor = scene.FindActorByID(actorID);
            if (!actor || !actor->HasComponent<Scene::DirectionalLightComponent>()) return false;
            return actor->RemoveComponent<Scene::DirectionalLightComponent>();
        });
    }

    bool EditorCommands::ResetDirectionalLight(Scene::Scene& scene, Core::UUID actorID)
    {
        return ApplyImmediateMutation(m_EditorController, scene, [&]()
        {
            Scene::Actor* actor = scene.FindActorByID(actorID);
            if (!actor) return false;

            Scene::DirectionalLightComponent* light = actor->TryGetComponent<Scene::DirectionalLightComponent>();
            if (!light) return false;
            *light = Scene::DirectionalLightComponent{};

            return true;
        });
    }

    bool EditorCommands::PasteDirectionalLightValues(Scene::Scene& scene, Core::UUID actorID, const Scene::DirectionalLightComponent& directionalLight)
    {
        if (!std::isfinite(directionalLight.Direction.x) ||
            !std::isfinite(directionalLight.Direction.y) ||
            !std::isfinite(directionalLight.Direction.z) ||
            !std::isfinite(directionalLight.Color.x) ||
            !std::isfinite(directionalLight.Color.y) ||
            !std::isfinite(directionalLight.Color.z) ||
            !std::isfinite(directionalLight.Intensity))
        {
            return false;
        }

        Scene::DirectionalLightComponent validatedLight = directionalLight;
        validatedLight.Direction =
        {
            std::clamp(directionalLight.Direction.x, -1.0f, 1.0f),
            std::clamp(directionalLight.Direction.y, -1.0f, 1.0f),
            std::clamp(directionalLight.Direction.z, -1.0f, 1.0f)
        };
        validatedLight.Color =
        {
            std::clamp(directionalLight.Color.x, 0.0f, 1.0f),
            std::clamp(directionalLight.Color.y, 0.0f, 1.0f),
            std::clamp(directionalLight.Color.z, 0.0f, 1.0f)
        };
        validatedLight.Intensity = std::clamp(directionalLight.Intensity, 0.0f, 10.0f);

        return ApplyImmediateMutation(m_EditorController, scene, [&]()
        {
            Scene::Actor* actor = scene.FindActorByID(actorID);
            if (!actor) return false;

            Scene::DirectionalLightComponent* light = actor->TryGetComponent<Scene::DirectionalLightComponent>();
            if (!light) return false;
            *light = validatedLight;

            return true;
        });
    }

    bool EditorCommands::SetDirectionalLightEnabled(Scene::Scene& scene, Core::UUID actorID, bool enabled)
    {
        return ApplyImmediateMutation(m_EditorController, scene, [&]()
        {
            Scene::Actor* actor = scene.FindActorByID(actorID);
            if (!actor) return false;

            Scene::DirectionalLightComponent* light = actor->TryGetComponent<Scene::DirectionalLightComponent>();
            if (!light) return false;
            light->Enabled = enabled;

            return true;
        });
    }

    bool EditorCommands::SetDirectionalLightDirection( Scene::Scene& scene, Core::UUID actorID, const glm::vec3& direction)
    {
        if (!CanEditScene() ||
            !std::isfinite(direction.x) ||
            !std::isfinite(direction.y) ||
            !std::isfinite(direction.z))
        {
            return false;
        }

        Scene::Actor* actor = scene.FindActorByID(actorID);
        if (!actor) return false;

        Scene::DirectionalLightComponent* light = actor->TryGetComponent<Scene::DirectionalLightComponent>();
        if (!light) return false;

        light->Direction =
        {
            std::clamp(direction.x, -1.0f, 1.0f),
            std::clamp(direction.y, -1.0f, 1.0f),
            std::clamp(direction.z, -1.0f, 1.0f)
        };

        return true;
    }

    bool EditorCommands::SetDirectionalLightColor(Scene::Scene& scene, Core::UUID actorID, const glm::vec3& color)
    {
        if (!CanEditScene() ||
            !std::isfinite(color.x) ||
            !std::isfinite(color.y) ||
            !std::isfinite(color.z))
        {
            return false;
        }

        Scene::Actor* actor = scene.FindActorByID(actorID);
        if (!actor) return false;

        Scene::DirectionalLightComponent* light = actor->TryGetComponent<Scene::DirectionalLightComponent>();
        if (!light) return false;

        light->Color =
        {
            std::clamp(color.x, 0.0f, 1.0f),
            std::clamp(color.y, 0.0f, 1.0f),
            std::clamp(color.z, 0.0f, 1.0f)
        };

        return true;
    }

    bool EditorCommands::SetDirectionalLightIntensity(Scene::Scene& scene, Core::UUID actorID, float intensity)
    {
        if (!CanEditScene() || !std::isfinite(intensity)) return false;

        Scene::Actor* actor = scene.FindActorByID(actorID);
        if (!actor) return false;

        Scene::DirectionalLightComponent* light = actor->TryGetComponent<Scene::DirectionalLightComponent>();
        if (!light) return false;

        light->Intensity = std::clamp(intensity, 0.0f, 10.0f);
        return true;
    }

#pragma endregion

#pragma region Rotator

    bool EditorCommands::AddRotator(Scene::Scene& scene, Core::UUID actorID, float initialSpeed)
    {
        if (!std::isfinite(initialSpeed)) return false;

        return ApplyImmediateMutation(m_EditorController, scene, [&]()
        {
            Scene::Actor* actor = scene.FindActorByID(actorID);
            if (!actor || actor->HasComponent<Scene::RotatorComponent>()) return false;

            actor->AddComponent<Scene::RotatorComponent>(Scene::RotatorComponent{ initialSpeed });
            return true;
        });
    }

    bool EditorCommands::RemoveRotator(Scene::Scene& scene, Core::UUID actorID)
    {
        return ApplyImmediateMutation(m_EditorController, scene, [&]()
        {
            Scene::Actor* actor = scene.FindActorByID(actorID);
            if (!actor || !actor->HasComponent<Scene::RotatorComponent>()) return false;

            return actor->RemoveComponent<Scene::RotatorComponent>();
        });
    }

    bool EditorCommands::ResetRotator(Scene::Scene& scene, Core::UUID actorID)
    {
        return ApplyImmediateMutation(m_EditorController, scene, [&]()
        {
            Scene::Actor* actor = scene.FindActorByID(actorID);
            if (!actor) return false;

            Scene::RotatorComponent* rotator = actor->TryGetComponent<Scene::RotatorComponent>();
            if (!rotator) return false;

            *rotator = Scene::RotatorComponent{ 45.0f };
            return true;
        });
    }

    bool EditorCommands::PasteRotatorValues(Scene::Scene& scene, Core::UUID actorID, const Scene::RotatorComponent& rotator)
    {
        if (!std::isfinite(rotator.RotationSpeedDegreesPerSecond)) return false;

        return ApplyImmediateMutation(m_EditorController, scene, [&]()
        {
            Scene::Actor* actor = scene.FindActorByID(actorID);
            if (!actor) return false;

            Scene::RotatorComponent* existingRotator = actor->TryGetComponent<Scene::RotatorComponent>();
            if (!existingRotator) return false;
            *existingRotator = rotator;

            return true;
        });
    }

    bool EditorCommands::SetRotatorSpeed(Scene::Scene& scene, Core::UUID actorID, float speed)
    {
        if (!CanEditScene() || !std::isfinite(speed)) return false;

        Scene::Actor* actor = scene.FindActorByID(actorID);
        if (!actor) return false;

        Scene::RotatorComponent* rotator = actor->TryGetComponent<Scene::RotatorComponent>();
        if (!rotator) return false;
        rotator->RotationSpeedDegreesPerSecond = speed;

        return true;
    }

#pragma endregion

#pragma region Scene Audio

    bool EditorCommands::SetSceneMasterVolume(Scene::Scene &scene, float volume)
    {
        if (!CanEditScene() || !std::isfinite(volume)) return false;
        scene.GetAudioSettings().MasterVolume = std::clamp(volume, 0.0f, 4.0f);
        return true;
    }

    bool EditorCommands::SetSceneMusicVolume(Scene::Scene &scene, float volume)
    {
        if (!CanEditScene() || !std::isfinite(volume)) return false;
        scene.GetAudioSettings().MusicVolume = std::clamp(volume, 0.0f, 4.0f);
        return true;
    }

    bool EditorCommands::SetSceneSFXVolume(Scene::Scene &scene, float volume)
    {
        if (!CanEditScene() || !std::isfinite(volume)) return false;
        scene.GetAudioSettings().SFXVolume = std::clamp(volume, 0.0f, 4.0f);
        return true;
    }

    bool EditorCommands::SetSceneUIVolume(Scene::Scene &scene, float volume)
    {
        if (!CanEditScene() || !std::isfinite(volume)) return false;
        scene.GetAudioSettings().UIVolume = std::clamp(volume, 0.0f, 4.0f);
        return true;
    }

#pragma endregion

#pragma region Scene Lighting

    bool EditorCommands::SetSceneLightDirection(Scene::Scene& scene, const glm::vec3& direction)
    {
        if (!CanEditScene()) return false;
        scene.GetRenderSettings().LightDirection = direction;
        return true;
    }

    bool EditorCommands::SetSceneLightIntensity(Scene::Scene& scene, const float intensity)
    {
        if (!CanEditScene()) return false;
        scene.GetRenderSettings().LightIntensity = intensity;
        return true;
    }

    bool EditorCommands::SetSceneAmbientColor(Scene::Scene& scene, const glm::vec3& color)
    {
        if (!CanEditScene()) return false;
        scene.GetRenderSettings().AmbientColor = color;
        return true;
    }

    bool EditorCommands::SetSceneAmbientIntensity(Scene::Scene& scene, const float intensity)
    {
        if (!CanEditScene()) return false;
        scene.GetRenderSettings().AmbientIntensity = intensity;
        return true;
    }

#pragma endregion

#pragma region Scene Physics

    bool EditorCommands::SetSceneGravity(Scene::Scene &scene, const glm::vec3 &gravity)
    {
        if (!CanEditScene() ||
            !std::isfinite(gravity.x) ||
            !std::isfinite(gravity.y) ||
            !std::isfinite(gravity.z))
        {
            return false;
        }

        scene.GetPhysicsSettings().Gravity = gravity;
        return true;
    }

#pragma endregion

#pragma region Camera

    bool EditorCommands::AddCamera(Scene::Scene& scene, Core::UUID actorID)
    {
        return ApplyImmediateMutation(m_EditorController, scene, [&]()
        {
            Scene::Actor* actor = scene.FindActorByID(actorID);
            if (!actor || actor->HasComponent<Scene::CameraComponent>()) return false;

            Scene::CameraComponent camera{};
            camera.Primary = scene.FindPrimaryCameraActor() == nullptr;

            actor->AddComponent<Scene::CameraComponent>(camera);
            return true;
        });
    }

    bool EditorCommands::RemoveCamera(Scene::Scene& scene, Core::UUID actorID)
    {
        return ApplyImmediateMutation(m_EditorController, scene, [&]()
        {
            Scene::Actor* actor = scene.FindActorByID(actorID);
            if (!actor) return false;

            Scene::CameraComponent* camera = actor->TryGetComponent<Scene::CameraComponent>();
            if (!camera) return false;

            const bool wasPrimary = camera->Primary;
            actor->RemoveComponent<Scene::CameraComponent>();

            auto& editorState = m_EditorController.GetState();
            if (editorState.Selection.PilotedCameraActorID.has_value() && editorState.Selection.PilotedCameraActorID.value() == actorID)
            {
                editorState.Selection.PilotedCameraActorID.reset();
            }

            if (wasPrimary)
            {
                for (Scene::Actor& other : scene.GetActors())
                {
                    Scene::CameraComponent* otherCamera = other.TryGetComponent<Scene::CameraComponent>();
                    if (!otherCamera) continue;
                    otherCamera->Primary = true;

                    break;
                }
            }
            return true;
        });
    }

    bool EditorCommands::ResetCamera(Scene::Scene& scene, Core::UUID actorID)
    {
        return ApplyImmediateMutation(m_EditorController, scene, [&]()
        {
            Scene::Actor* actor = scene.FindActorByID(actorID);
            if (!actor) return false;

            Scene::CameraComponent* existingCamera = actor->TryGetComponent<Scene::CameraComponent>();
            if (!existingCamera) return false;

            const Scene::Actor* primaryCameraActor = scene.FindPrimaryCameraActor();

            Scene::CameraComponent camera{};
            camera.Primary = primaryCameraActor == nullptr || primaryCameraActor->GetID() == actorID;

            *existingCamera = camera;
            return true;
        });
    }

    bool EditorCommands::PasteCameraValues(Scene::Scene& scene, Core::UUID actorID, const Scene::CameraComponent& camera)
    {
        if (!std::isfinite(camera.FieldOfViewDegrees) ||
            !std::isfinite(camera.NearClip) ||
            !std::isfinite(camera.FarClip))
        {
            return false;
        }

        Scene::CameraComponent validatedCamera = camera;
        validatedCamera.FieldOfViewDegrees = std::clamp(camera.FieldOfViewDegrees, 1.0f, 179.0f);
        validatedCamera.NearClip = std::clamp(camera.NearClip, 0.001f, 100.0f);

        const float minimumFarClip = std::max(1.0f, validatedCamera.NearClip + 0.001f);
        validatedCamera.FarClip = std::clamp(camera.FarClip, minimumFarClip, 100000.0f);

        return ApplyImmediateMutation(m_EditorController, scene, [&]()
        {
            Scene::Actor* actor = scene.FindActorByID(actorID);
            if (!actor) return false;

            Scene::CameraComponent* existingCamera = actor->TryGetComponent<Scene::CameraComponent>();
            if (!existingCamera) return false;

            if (validatedCamera.Primary)
            {
                for (Scene::Actor& other : scene.GetActors())
                {
                    Scene::CameraComponent* otherCamera = other.TryGetComponent<Scene::CameraComponent>();

                    if (otherCamera)
                    {
                        otherCamera->Primary = false;
                    }
                }
            }

            *existingCamera = validatedCamera;
            return true;
        });
    }

    bool EditorCommands::SetCameraPrimary(Scene::Scene& scene, Core::UUID actorID, bool primary)
    {
        return ApplyImmediateMutation(m_EditorController, scene, [&]()
        {
            Scene::Actor* actor = scene.FindActorByID(actorID);
            if (!actor) return false;

            Scene::CameraComponent* camera = actor->TryGetComponent<Scene::CameraComponent>();
            if (!camera) return false;

            if (primary)
            {
                for (Scene::Actor& other : scene.GetActors())
                {
                    Scene::CameraComponent* otherCamera = other.TryGetComponent<Scene::CameraComponent>();
                    if (otherCamera)
                    {
                        otherCamera->Primary = false;
                    }
                }
            }

            camera->Primary = primary;
            return true;
        });
    }

    bool EditorCommands::SetCameraFieldOfView(Scene::Scene& scene, Core::UUID actorID, float fovDegrees)
    {
        if (!CanEditScene() || !std::isfinite(fovDegrees)) return false;

        Scene::Actor* actor =  scene.FindActorByID(actorID);
        if (!actor) return false;

        Scene::CameraComponent* camera = actor->TryGetComponent<Scene::CameraComponent>();
        if (!camera) return false;

        camera->FieldOfViewDegrees = std::clamp(fovDegrees, 1.0f, 179.0f);
        return true;
    }

    bool EditorCommands::SetCameraNearClip(Scene::Scene& scene, Core::UUID actorID, float nearClip)
    {
        if (!CanEditScene() || !std::isfinite(nearClip)) return false;

        Scene::Actor* actor = scene.FindActorByID(actorID);
        if (!actor) return false;

        Scene::CameraComponent* camera = actor->TryGetComponent<Scene::CameraComponent>();
        if (!camera) return false;

        float validatedFarClip = camera->FarClip;
        if (!std::isfinite(validatedFarClip))
        {
            validatedFarClip = 1000.0f;
        }

        validatedFarClip = std::clamp(validatedFarClip, 1.0f, 100000.0f);
        const float maximumNearClip = std::clamp(validatedFarClip - 0.001f, 0.001f, 100.0f);

        camera->FarClip = validatedFarClip;
        camera->NearClip = std::clamp(nearClip, 0.001f, maximumNearClip);
        return true;
    }

    bool EditorCommands::SetCameraFarClip(Scene::Scene& scene, Core::UUID actorID, float farClip)
    {
        if (!CanEditScene() || !std::isfinite(farClip)) return false;

        Scene::Actor* actor = scene.FindActorByID(actorID);
        if (!actor) return false;

        Scene::CameraComponent* camera = actor->TryGetComponent<Scene::CameraComponent>();
        if (!camera) return false;

        float validatedNearClip = camera->NearClip;
        if (!std::isfinite(validatedNearClip))
        {
            validatedNearClip = 0.1f;
        }

        validatedNearClip = std::clamp(validatedNearClip, 0.001f, 100.0f);
        const float minimumFarClip = std::max(1.0f, validatedNearClip + 0.001f);

        camera->NearClip = validatedNearClip;
        camera->FarClip = std::clamp(farClip, minimumFarClip, 100000.0f);
        return true;
    }

 #pragma endregion

#pragma region Audio

    bool EditorCommands::AddAudioSource(Scene::Scene& scene, const Core::UUID actorID)
    {
        return ApplyImmediateMutation(m_EditorController, scene, [&]()
        {
            Scene::Actor* actor = scene.FindActorByID(actorID);
            if (!actor || actor->HasComponent<Scene::AudioSourceComponent>()) return false;

            actor->AddComponent<Scene::AudioSourceComponent>();
            return true;
        });
    }

    bool EditorCommands::RemoveAudioSource(Scene::Scene& scene, const Core::UUID actorID)
    {
        return ApplyImmediateMutation(m_EditorController, scene, [&]()
        {
            Scene::Actor* actor = scene.FindActorByID(actorID);
            if (!actor || !actor->HasComponent<Scene::AudioSourceComponent>()) return false;

            return actor->RemoveComponent<Scene::AudioSourceComponent>();
        });
    }

    bool EditorCommands::ResetAudioSource(Scene::Scene& scene, const Core::UUID actorID)
    {
        return ApplyImmediateMutation(m_EditorController, scene, [&]()
        {
            Scene::Actor* actor = scene.FindActorByID(actorID);
            if (!actor) return false;

            auto* audioSource = actor->TryGetComponent<Scene::AudioSourceComponent>();
            if (!audioSource) return false;
            *audioSource = Scene::AudioSourceComponent{};

            return true;
        });
    }

    bool EditorCommands::PasteAudioSourceValues(Scene::Scene& scene, const Core::UUID actorID, const Scene::AudioSourceComponent& audioSource, const Project::ProjectContext& projectContext)
    {
        if (!std::isfinite(audioSource.Volume) ||
            !std::isfinite(audioSource.Pitch) ||
            !std::isfinite(audioSource.MinDistance) ||
            !std::isfinite(audioSource.MaxDistance) ||
            !std::isfinite(audioSource.Rolloff) ||
            !std::isfinite(audioSource.ConeInnerAngleDegrees) ||
            !std::isfinite(audioSource.ConeOuterAngleDegrees) ||
            !std::isfinite(audioSource.ConeOuterGain) ||
            !std::isfinite(audioSource.DopplerFactor))
            return false;

        if (!audioSource.AudioAssetID.empty())
        {
            const Project::AssetRecord* record = Project::AssetRegistryIO::FindByID(projectContext.GetAssetRegistry(), audioSource.AudioAssetID);
            if (!record || record->Type != Project::AssetType::Audio) return false;
        }

        switch (audioSource.Bus)
        {
            case Audio::AudioBus::Master:
            case Audio::AudioBus::Music:
            case Audio::AudioBus::SFX:
            case Audio::AudioBus::UI:
                break;
            default:
                return false;
        }

        Scene::AudioSourceComponent validated = audioSource;
        validated.Volume = std::clamp(audioSource.Volume, 0.0f, 4.0f);
        validated.Pitch = std::clamp(audioSource.Pitch, 0.01f, 4.0f);
        validated.MinDistance = std::clamp(audioSource.MinDistance, 0.01f, 99999.99f);
        validated.MaxDistance = std::clamp(audioSource.MaxDistance, validated.MinDistance + 0.01f, 100000.0f);
        validated.Rolloff = std::clamp(audioSource.Rolloff, 0.0f, 10.0f);
        validated.ConeInnerAngleDegrees = std::clamp(audioSource.ConeInnerAngleDegrees, 0.0f, 360.0f);
        validated.ConeOuterAngleDegrees = std::clamp(audioSource.ConeOuterAngleDegrees, validated.ConeInnerAngleDegrees, 360.0f);
        validated.ConeOuterGain = std::clamp(audioSource.ConeOuterGain, 0.0f, 1.0f);
        validated.DopplerFactor = std::clamp(audioSource.DopplerFactor, 0.0f, 10.0f);

        return ApplyImmediateMutation(m_EditorController, scene, [&]()
        {
            Scene::Actor* actor = scene.FindActorByID(actorID);
            if (!actor) return false;

            auto* existing = actor->TryGetComponent<Scene::AudioSourceComponent>();
            if (!existing) return false;

            *existing = validated;
            return true;
        });
    }


    bool EditorCommands::SetAudioSourceAsset(Scene::Scene& scene, const Core::UUID actorID, const std::string& audioAssetID, const Project::ProjectContext& projectContext)
    {
        if (!audioAssetID.empty())
        {
            const Project::AssetRecord* record = Project::AssetRegistryIO::FindByID(projectContext.GetAssetRegistry(), audioAssetID);
            if (!record || record->Type != Project::AssetType::Audio) return false;
        }

        return ApplyImmediateMutation(m_EditorController, scene, [&]()
        {
            Scene::Actor* actor = scene.FindActorByID(actorID);
            if (!actor) return false;

            auto* audioSource = actor->TryGetComponent<Scene::AudioSourceComponent>();
            if (!audioSource || audioSource->AudioAssetID == audioAssetID) return false;
            audioSource->AudioAssetID = audioAssetID;

            return true;
        });
    }

    bool EditorCommands::SetAudioSourceBus(Scene::Scene& scene, const Core::UUID actorID, const Audio::AudioBus bus)
    {
        switch (bus)
        {
            case Audio::AudioBus::Master:
            case Audio::AudioBus::Music:
            case Audio::AudioBus::SFX:
            case Audio::AudioBus::UI:
                break;
            default:
                return false;
        }

        return ApplyImmediateMutation(m_EditorController, scene, [&]()
        {
            Scene::Actor* actor = scene.FindActorByID(actorID);
            if (!actor) return false;

            auto* audioSource = actor->TryGetComponent<Scene::AudioSourceComponent>();
            if (!audioSource || audioSource->Bus == bus) return false;

            audioSource->Bus = bus;
            return true;
        });
    }

    bool EditorCommands::SetAudioSourceVolume(Scene::Scene& scene, const Core::UUID actorID, const float volume)
    {
        if (!CanEditScene() || !std::isfinite(volume)) return false;

        Scene::Actor* actor = scene.FindActorByID(actorID);
        if (!actor) return false;

        auto* audioSource = actor->TryGetComponent<Scene::AudioSourceComponent>();
        if (!audioSource) return false;
        audioSource->Volume = std::clamp(volume, 0.0f, 4.0f);

        return true;
    }

    bool EditorCommands::SetAudioSourcePitch(Scene::Scene& scene, const Core::UUID actorID, const float pitch)
    {
        if (!CanEditScene() || !std::isfinite(pitch)) return false;

        Scene::Actor* actor = scene.FindActorByID(actorID);
        if (!actor) return false;

        auto* audioSource = actor->TryGetComponent<Scene::AudioSourceComponent>();
        if (!audioSource) return false;
        audioSource->Pitch = std::clamp(pitch, 0.01f, 4.0f);

        return true;
    }

    bool EditorCommands::SetAudioSourcePlayOnStart(Scene::Scene& scene, const Core::UUID actorID, const bool playOnStart)
    {
        return ApplyImmediateMutation(m_EditorController, scene, [&]()
        {
            Scene::Actor* actor = scene.FindActorByID(actorID);
            if (!actor) return false;

            auto* audioSource = actor->TryGetComponent<Scene::AudioSourceComponent>();
            if (!audioSource) return false;
            audioSource->PlayOnStart = playOnStart;

            return true;
        });
    }

    bool EditorCommands::SetAudioSourceLoop(Scene::Scene& scene, const Core::UUID actorID, const bool loop)
    {
        return ApplyImmediateMutation(m_EditorController, scene, [&]()
        {
            Scene::Actor* actor = scene.FindActorByID(actorID);
            if (!actor) return false;

            auto* audioSource = actor->TryGetComponent<Scene::AudioSourceComponent>();
            if (!audioSource) return false;
            audioSource->Loop = loop;

            return true;
        });
    }

    bool EditorCommands::SetAudioSourceSpatial(Scene::Scene& scene, const Core::UUID actorID, const bool spatial)
    {
        return ApplyImmediateMutation(m_EditorController, scene, [&]()
        {
            Scene::Actor* actor = scene.FindActorByID(actorID);
            if (!actor) return false;

            auto* audioSource = actor->TryGetComponent<Scene::AudioSourceComponent>();
            if (!audioSource) return false;
            audioSource->Spatial = spatial;

            return true;
        });
    }

    bool EditorCommands::SetAudioSourceMinDistance(Scene::Scene& scene, const Core::UUID actorID, const float minDistance)
    {
        if (!CanEditScene() || !std::isfinite(minDistance)) return false;

        Scene::Actor* actor = scene.FindActorByID(actorID);
        if (!actor) return false;

        auto* audioSource = actor->TryGetComponent<Scene::AudioSourceComponent>();
        if (!audioSource) return false;

        float validatedMaxDistance = audioSource->MaxDistance;
        if (!std::isfinite(validatedMaxDistance))
        {
            validatedMaxDistance = 100000.0f;
        }

        validatedMaxDistance = std::clamp(validatedMaxDistance, 0.02f, 100000.0f);
        const float maximumMinDistance = std::clamp(validatedMaxDistance - 0.01f, 0.01f, 99999.99f);

        audioSource->MaxDistance = validatedMaxDistance;
        audioSource->MinDistance = std::clamp(minDistance, 0.01f, maximumMinDistance);
        return true;
    }

    bool EditorCommands::SetAudioSourceMaxDistance(Scene::Scene& scene, const Core::UUID actorID, const float maxDistance)
    {
        if (!CanEditScene() || !std::isfinite(maxDistance)) return false;

        Scene::Actor* actor = scene.FindActorByID(actorID);
        if (!actor) return false;

        auto* audioSource = actor->TryGetComponent<Scene::AudioSourceComponent>();
        if (!audioSource) return false;

        float validatedMinDistance = audioSource->MinDistance;
        if (!std::isfinite(validatedMinDistance))
        {
            validatedMinDistance = 1.0f;
        }
        validatedMinDistance = std::clamp(validatedMinDistance, 0.01f, 99999.99f);

        const float minimumMaxDistance = std::min(100000.0f, validatedMinDistance + 0.01f);
        audioSource->MinDistance = validatedMinDistance;
        audioSource->MaxDistance = std::clamp(maxDistance, minimumMaxDistance, 100000.0f);
        return true;
    }

    bool EditorCommands::SetAudioSourceRolloff(Scene::Scene& scene, const Core::UUID actorID, const float rolloff)
    {
        if (!CanEditScene() || !std::isfinite(rolloff)) return false;

        Scene::Actor* actor = scene.FindActorByID(actorID);
        if (!actor) return false;

        auto* audioSource = actor->TryGetComponent<Scene::AudioSourceComponent>();
        if (!audioSource) return false;
        audioSource->Rolloff = std::clamp(rolloff, 0.0f, 10.0f);
        return true;
    }

    bool EditorCommands::SetAudioSourceConeInnerAngle(Scene::Scene& scene, const Core::UUID actorID, const float angleDegrees)
    {
        if (!CanEditScene() || !std::isfinite(angleDegrees)) return false;

        Scene::Actor* actor = scene.FindActorByID(actorID);
        if (!actor) return false;

        auto* audioSource = actor->TryGetComponent<Scene::AudioSourceComponent>();
        if (!audioSource) return false;

        const float outerAngle = std::isfinite(audioSource->ConeOuterAngleDegrees) ? std::clamp(audioSource->ConeOuterAngleDegrees, 0.0f, 360.0f) : 360.0f;
        audioSource->ConeOuterAngleDegrees = outerAngle;
        audioSource->ConeInnerAngleDegrees = std::clamp(angleDegrees, 0.0f, outerAngle);
        return true;
    }

    bool EditorCommands::SetAudioSourceConeOuterAngle(Scene::Scene& scene, const Core::UUID actorID, const float angleDegrees)
    {
        if (!CanEditScene() || !std::isfinite(angleDegrees)) return false;

        Scene::Actor* actor = scene.FindActorByID(actorID);
        if (!actor) return false;

        auto* audioSource = actor->TryGetComponent<Scene::AudioSourceComponent>();
        if (!audioSource) return false;

        const float innerAngle = std::isfinite(audioSource->ConeInnerAngleDegrees) ? std::clamp(audioSource->ConeInnerAngleDegrees, 0.0f, 360.0f) : 360.0f;
        audioSource->ConeInnerAngleDegrees = innerAngle;
        audioSource->ConeOuterAngleDegrees = std::clamp(angleDegrees, innerAngle, 360.0f);
        return true;
    }

    bool EditorCommands::SetAudioSourceConeOuterGain(Scene::Scene& scene, const Core::UUID actorID, const float gain)
    {
        if (!CanEditScene() || !std::isfinite(gain)) return false;

        Scene::Actor* actor = scene.FindActorByID(actorID);
        if (!actor) return false;

        auto* audioSource = actor->TryGetComponent<Scene::AudioSourceComponent>();
        if (!audioSource) return false;
        audioSource->ConeOuterGain = std::clamp(gain, 0.0f, 1.0f);
        return true;
    }

    bool EditorCommands::SetAudioSourceDopplerFactor(Scene::Scene &scene, Core::UUID actorID, float dopplerFactor)
    {
        if (!CanEditScene() || !std::isfinite(dopplerFactor)) return false;

        Scene::Actor* actor = scene.FindActorByID(actorID);
        if (!actor) return false;

        auto* audioSource = actor->TryGetComponent<Scene::AudioSourceComponent>();
        if (!audioSource) return false;
        audioSource->DopplerFactor = std::clamp(dopplerFactor, 0.0f, 10.0f);
        return true;
    }


    bool EditorCommands::AddAudioListener(Scene::Scene& scene, const Core::UUID actorID)
    {
        return ApplyImmediateMutation(m_EditorController, scene, [&]()
        {
            Scene::Actor* actor = scene.FindActorByID(actorID);
            if (!actor || actor->HasComponent<Scene::AudioListenerComponent>()) return false;

            Scene::AudioListenerComponent listener{};
            listener.Primary = scene.FindPrimaryAudioListenerActor() == nullptr;

            actor->AddComponent<Scene::AudioListenerComponent>(listener);
            return true;
        });
    }

    bool EditorCommands::RemoveAudioListener(Scene::Scene& scene, const Core::UUID actorID)
    {
        return ApplyImmediateMutation(m_EditorController, scene, [&]()
        {
            Scene::Actor* actor = scene.FindActorByID(actorID);
            if (!actor) return false;

            Scene::AudioListenerComponent* listener = actor->TryGetComponent<Scene::AudioListenerComponent>();
            if (!listener) return false;
            const bool wasPrimary = listener->Primary;

            actor->RemoveComponent<Scene::AudioListenerComponent>();
            if (wasPrimary)
            {
                for (Scene::Actor& other : scene.GetActors())
                {
                    auto* otherListener = other.TryGetComponent<Scene::AudioListenerComponent>();
                    if (!otherListener) continue;
                    otherListener->Primary = true;
                    break;
                }
            }
            return true;
        });
    }

    bool EditorCommands::ResetAudioListener(Scene::Scene& scene, const Core::UUID actorID)
    {
        return ApplyImmediateMutation(m_EditorController, scene, [&]()
        {
            Scene::Actor* actor = scene.FindActorByID(actorID);
            if (!actor) return false;

            auto* listener = actor->TryGetComponent<Scene::AudioListenerComponent>();
            if (!listener) return false;

            const Scene::Actor* primaryListener = scene.FindPrimaryAudioListenerActor();
            Scene::AudioListenerComponent reset{};

            reset.Primary = primaryListener == nullptr || primaryListener->GetID() == actorID;
            *listener = reset;
            return true;
        });
    }

    bool EditorCommands::PasteAudioListenerValues(Scene::Scene& scene, const Core::UUID actorID, const Scene::AudioListenerComponent& audioListener)
    {
        return ApplyImmediateMutation(m_EditorController, scene, [&]()
        {
            Scene::Actor* actor = scene.FindActorByID(actorID);
            if (!actor) return false;

            auto* existing = actor->TryGetComponent<Scene::AudioListenerComponent>();
            if (!existing) return false;

            if (audioListener.Primary)
            {
                for (Scene::Actor& other : scene.GetActors())
                {
                    auto* otherListener = other.TryGetComponent<Scene::AudioListenerComponent>();
                    if (otherListener) otherListener->Primary = false;
                }
            }
            *existing = audioListener;
            return true;
        });
    }

    bool EditorCommands::SetAudioListenerPrimary(Scene::Scene& scene, const Core::UUID actorID, const bool primary)
    {
        return ApplyImmediateMutation(m_EditorController, scene, [&]()
        {
            Scene::Actor* actor = scene.FindActorByID(actorID);
            if (!actor) return false;

            auto* listener = actor->TryGetComponent<Scene::AudioListenerComponent>();
            if (!listener) return false;

            if (primary)
            {
                for (Scene::Actor& other : scene.GetActors())
                {
                    auto* otherListener = other.TryGetComponent<Scene::AudioListenerComponent>();
                    if (otherListener) otherListener->Primary = false;
                }
            }

            listener->Primary = primary;
            return true;
        });
    }

#pragma endregion

#pragma region Physics

    bool EditorCommands::AddRigidBody(Scene::Scene &scene, Core::UUID actorID)
    {
        return ApplyImmediateMutation(m_EditorController, scene, [&]()
        {
            Scene::Actor* actor = scene.FindActorByID(actorID);
            if (!actor || actor->HasComponent<Scene::RigidBodyComponent>()) return false;

            actor->AddComponent<Scene::RigidBodyComponent>();
            return true;
        });
    }

    bool EditorCommands::RemoveRigidBody(Scene::Scene &scene, Core::UUID actorID)
    {
        return ApplyImmediateMutation(m_EditorController, scene, [&]()
        {
            Scene::Actor* actor = scene.FindActorByID(actorID);
            if (!actor || !actor->HasComponent<Scene::RigidBodyComponent>()) return false;

            return actor->RemoveComponent<Scene::RigidBodyComponent>();
        });
    }

    bool EditorCommands::ResetRigidBody(Scene::Scene &scene, Core::UUID actorID)
    {
        return ApplyImmediateMutation(m_EditorController, scene, [&]()
        {
            Scene::Actor* actor = scene.FindActorByID(actorID);
            if (!actor) return false;

            auto* rigidBody = actor->TryGetComponent<Scene::RigidBodyComponent>();
            if (!rigidBody) return false;
            *rigidBody = Scene::RigidBodyComponent{};
            return true;
        });
    }

    bool EditorCommands::PasteRigidBodyValues(Scene::Scene &scene, Core::UUID actorID, const Scene::RigidBodyComponent &rigidBody)
    {
        if (!std::isfinite(rigidBody.Friction) ||
            !std::isfinite(rigidBody.Restitution) ||
            !std::isfinite(rigidBody.LinearDamping) ||
            !std::isfinite(rigidBody.AngularDamping) ||
            !std::isfinite(rigidBody.GravityFactor))
        {
            return false;
        }

        Scene::RigidBodyComponent validated = rigidBody;
        validated.Friction = std::max(rigidBody.Friction, 0.0f);
        validated.Restitution = std::clamp(rigidBody.Restitution, 0.0f, 1.0f);
        validated.LinearDamping = std::max(rigidBody.LinearDamping, 0.0f);
        validated.AngularDamping = std::max(rigidBody.AngularDamping, 0.0f);

        return ApplyImmediateMutation(m_EditorController, scene, [&]()
        {
            Scene::Actor* actor = scene.FindActorByID(actorID);
            if (!actor) return false;

            auto* existing = actor->TryGetComponent<Scene::RigidBodyComponent>();
            if (!existing) return false;
            *existing = validated;
            return true;
        });
    }

    bool EditorCommands::SetRigidBodyMotionType(Scene::Scene &scene, Core::UUID actorID, Scene::RigidBodyMotionType motionType)
    {
        switch (motionType)
        {
            case Scene::RigidBodyMotionType::Static:
            case Scene::RigidBodyMotionType::Dynamic:
            case Scene::RigidBodyMotionType::Kinematic:
                break;

            default:
                return false;
        }

        return ApplyImmediateMutation(m_EditorController, scene, [&]()
        {
            Scene::Actor* actor = scene.FindActorByID(actorID);
            if (!actor) return false;

            auto* rigidBody = actor->TryGetComponent<Scene::RigidBodyComponent>();
            if (!rigidBody) return false;
            if (rigidBody->MotionType == motionType) return false;
            rigidBody->MotionType = motionType;

            return true;
        });
    }

    bool EditorCommands::SetRigidBodyFriction(Scene::Scene& scene, Core::UUID actorID, float friction)
    {
        if (!CanEditScene() || !std::isfinite(friction)) return false;

        Scene::Actor* actor = scene.FindActorByID(actorID);
        if (!actor) return false;

        auto* rigidBody = actor->TryGetComponent<Scene::RigidBodyComponent>();
        if (!rigidBody) return false;
        rigidBody->Friction = std::max(friction, 0.0f);

        return true;
    }

    bool EditorCommands::SetRigidBodyRestitution(Scene::Scene &scene, Core::UUID actorID, float restitution)
    {
        if (!CanEditScene() || !std::isfinite(restitution)) return false;

        Scene::Actor* actor = scene.FindActorByID(actorID);
        if (!actor) return false;

        auto* rigidBody = actor->TryGetComponent<Scene::RigidBodyComponent>();
        if (!rigidBody) return false;
        rigidBody->Restitution = std::clamp(restitution, 0.0f, 1.0f);

        return true;
    }

    bool EditorCommands::SetRigidBodyLinearDamping(Scene::Scene &scene, Core::UUID actorID, float damping)
    {
        if (!CanEditScene() || !std::isfinite(damping)) return false;

        Scene::Actor* actor = scene.FindActorByID(actorID);
        if (!actor) return false;

        auto* rigidBody = actor->TryGetComponent<Scene::RigidBodyComponent>();
        if (!rigidBody) return false;
        rigidBody->LinearDamping = std::max(damping, 0.0f);

        return true;
    }

    bool EditorCommands::SetRigidBodyAngularDamping(Scene::Scene &scene, Core::UUID actorID, float damping)
    {
        if (!CanEditScene() || !std::isfinite(damping)) return false;

        Scene::Actor* actor = scene.FindActorByID(actorID);
        if (!actor) return false;

        auto* rigidBody = actor->TryGetComponent<Scene::RigidBodyComponent>();
        if (!rigidBody) return false;
        rigidBody->AngularDamping = std::max(damping, 0.0f);

        return true;
    }

    bool EditorCommands::SetRigidBodyGravityFactor(Scene::Scene &scene, Core::UUID actorID, float gravityFactor)
    {
        if (!CanEditScene() || !std::isfinite(gravityFactor)) return false;

        Scene::Actor* actor = scene.FindActorByID(actorID);
        if (!actor) return false;

        auto* rigidBody = actor->TryGetComponent<Scene::RigidBodyComponent>();
        if (!rigidBody) return false;
         rigidBody->GravityFactor = gravityFactor;

        return true;
    }

    bool EditorCommands::SetRigidBodyAllowSleeping(Scene::Scene &scene, Core::UUID actorID, bool allowSleeping)
    {
        return ApplyImmediateMutation(m_EditorController, scene, [&]()
        {
            Scene::Actor* actor = scene.FindActorByID(actorID);

            if (!actor) return false;

            auto* rigidBody = actor->TryGetComponent<Scene::RigidBodyComponent>();
            if (!rigidBody || rigidBody->AllowSleeping == allowSleeping) return false;
            rigidBody->AllowSleeping = allowSleeping;

            return true;
        });
    }

    #pragma region BoxCollider

    bool EditorCommands::AddBoxCollider(Scene::Scene &scene, Core::UUID actorID)
    {
        return ApplyImmediateMutation(m_EditorController, scene, [&]()
        {
            Scene::Actor* actor = scene.FindActorByID(actorID);
            if (!actor ||
                actor->HasComponent<Scene::BoxColliderComponent>() ||
                actor->HasComponent<Scene::SphereColliderComponent>() ||
                actor->HasComponent<Scene::CapsuleColliderComponent>())
            {
                return false;
            }

            actor->AddComponent<Scene::BoxColliderComponent>();
            return true;
        });
    }

    bool EditorCommands::RemoveBoxCollider(Scene::Scene &scene, Core::UUID actorID)
    {
        return ApplyImmediateMutation(m_EditorController, scene, [&]()
        {
            Scene::Actor* actor = scene.FindActorByID(actorID);
            if (!actor || !actor->HasComponent<Scene::BoxColliderComponent>()) return false;

            return actor->RemoveComponent<Scene::BoxColliderComponent>();
        });
    }

    bool EditorCommands::ResetBoxCollider(Scene::Scene &scene, Core::UUID actorID)
    {
        return ApplyImmediateMutation(m_EditorController, scene, [&]()
        {
            Scene::Actor* actor = scene.FindActorByID(actorID);
            if (!actor) return false;

            auto* collider = actor->TryGetComponent<Scene::BoxColliderComponent>();
            if (!collider) return false;
            *collider = Scene::BoxColliderComponent{};

            return true;
        });
    }

    bool EditorCommands::PasteBoxColliderValues(Scene::Scene &scene, Core::UUID actorID, const Scene::BoxColliderComponent &collider)
    {
        if (!std::isfinite(collider.Size.x) ||
           !std::isfinite(collider.Size.y) ||
           !std::isfinite(collider.Size.z) ||
           !std::isfinite(collider.Center.x) ||
           !std::isfinite(collider.Center.y) ||
           !std::isfinite(collider.Center.z))
        {
            return false;
        }

        Scene::BoxColliderComponent validated = collider;
        validated.Size =
        {
            std::clamp(collider.Size.x, 0.001f, 100000.0f),
            std::clamp(collider.Size.y, 0.001f, 100000.0f),
            std::clamp(collider.Size.z, 0.001f, 100000.0f)
        };

        return ApplyImmediateMutation(m_EditorController, scene, [&]()
        {
            Scene::Actor* actor = scene.FindActorByID(actorID);
            if (!actor) return false;

            auto* existing = actor->TryGetComponent<Scene::BoxColliderComponent>();
            if (!existing) return false;
            *existing = validated;

            return true;
        });
    }


    bool EditorCommands::SetBoxColliderSize(Scene::Scene &scene, Core::UUID actorID, const glm::vec3 &size)
    {
        if (!CanEditScene() ||
            !std::isfinite(size.x) ||
            !std::isfinite(size.y) ||
            !std::isfinite(size.z))
        {
            return false;
        }

        Scene::Actor* actor = scene.FindActorByID(actorID);
        if (!actor) return false;

        auto* collider = actor->TryGetComponent<Scene::BoxColliderComponent>();
        if (!collider) return false;
        collider->Size =
        {
            std::clamp(size.x, 0.001f, 100000.0f),
            std::clamp(size.y, 0.001f, 100000.0f),
            std::clamp(size.z, 0.001f, 100000.0f)
        };

        return true;
    }

    bool EditorCommands::SetBoxColliderCenter(Scene::Scene &scene, Core::UUID actorID, const glm::vec3 &center)
    {
        if (!CanEditScene() ||
            !std::isfinite(center.x) ||
            !std::isfinite(center.y) ||
            !std::isfinite(center.z))
        {
            return false;
        }

        Scene::Actor* actor = scene.FindActorByID(actorID);
        if (!actor) return false;

        auto* collider = actor->TryGetComponent<Scene::BoxColliderComponent>();
        if (!collider) return false;
        collider->Center = center;

        return true;
    }

    #pragma endregion

    #pragma region SphereCollider

    bool EditorCommands::AddSphereCollider(Scene::Scene& scene, Core::UUID actorID)
    {
        return ApplyImmediateMutation(m_EditorController, scene, [&]()
        {
            Scene::Actor* actor = scene.FindActorByID(actorID);
            if (!actor ||
                actor->HasComponent<Scene::SphereColliderComponent>() ||
                actor->HasComponent<Scene::BoxColliderComponent>() ||
                actor->HasComponent<Scene::CapsuleColliderComponent>())
            {
                return false;
            }

            actor->AddComponent<Scene::SphereColliderComponent>();
            return true;
        });
    }

    bool EditorCommands::RemoveSphereCollider(Scene::Scene& scene, Core::UUID actorID)
    {
        return ApplyImmediateMutation(m_EditorController, scene, [&]()
        {
            Scene::Actor* actor = scene.FindActorByID(actorID);
            if (!actor || !actor->HasComponent<Scene::SphereColliderComponent>()) return false;

            return actor->RemoveComponent<Scene::SphereColliderComponent>();
        });
    }

    bool EditorCommands::ResetSphereCollider(Scene::Scene& scene, Core::UUID actorID)
    {
        return ApplyImmediateMutation(m_EditorController, scene, [&]()
        {
            Scene::Actor* actor = scene.FindActorByID(actorID);
            if (!actor) return false;

            auto* collider = actor->TryGetComponent<Scene::SphereColliderComponent>();
            if (!collider) return false;
            *collider = Scene::SphereColliderComponent{};

            return true;
        });
    }

    bool EditorCommands::PasteSphereColliderValues(Scene::Scene& scene, Core::UUID actorID, const Scene::SphereColliderComponent& collider)
    {
        if (!std::isfinite(collider.Radius) ||
            !std::isfinite(collider.Center.x) ||
            !std::isfinite(collider.Center.y) ||
            !std::isfinite(collider.Center.z))
        {
            return false;
        }

        Scene::SphereColliderComponent validated = collider;
        validated.Radius = std::clamp(collider.Radius, 0.001f, 100000.0f);

        return ApplyImmediateMutation(m_EditorController, scene, [&]()
        {
            Scene::Actor* actor = scene.FindActorByID(actorID);
            if (!actor) return false;

            auto* existing = actor->TryGetComponent<Scene::SphereColliderComponent>();
            if (!existing) return false;
            *existing = validated;

            return true;
        });
    }

    bool EditorCommands::SetSphereColliderRadius(Scene::Scene& scene, Core::UUID actorID, const float radius)
    {
        if (!CanEditScene() || !std::isfinite(radius)) return false;

        Scene::Actor* actor = scene.FindActorByID(actorID);
        if (!actor) return false;

        auto* collider = actor->TryGetComponent<Scene::SphereColliderComponent>();
        if (!collider) return false;
        collider->Radius = std::clamp(radius, 0.001f, 100000.0f);

        return true;
    }

    bool EditorCommands::SetSphereColliderCenter(Scene::Scene& scene, Core::UUID actorID, const glm::vec3& center)
    {
        if (!CanEditScene() ||
            !std::isfinite(center.x) ||
            !std::isfinite(center.y) ||
            !std::isfinite(center.z))
        {
            return false;
        }

        Scene::Actor* actor = scene.FindActorByID(actorID);
        if (!actor) return false;

        auto* collider = actor->TryGetComponent<Scene::SphereColliderComponent>();
        if (!collider) return false;
        collider->Center = center;

        return true;
    }

    #pragma endregion

    #pragma region CapsuleCollider

    bool EditorCommands::AddCapsuleCollider(Scene::Scene& scene, Core::UUID actorID)
    {
        return ApplyImmediateMutation(m_EditorController, scene, [&]()
        {
            Scene::Actor* actor = scene.FindActorByID(actorID);

            if (!actor ||
                actor->HasComponent<Scene::BoxColliderComponent>() ||
                actor->HasComponent<Scene::SphereColliderComponent>() ||
                actor->HasComponent<Scene::CapsuleColliderComponent>())
            {
                return false;
            }

            actor->AddComponent<Scene::CapsuleColliderComponent>();
            return true;
        });
    }

    bool EditorCommands::RemoveCapsuleCollider(Scene::Scene& scene, Core::UUID actorID)
    {
        return ApplyImmediateMutation(m_EditorController, scene, [&]()
        {
            Scene::Actor* actor = scene.FindActorByID(actorID);
            if (!actor || !actor->HasComponent<Scene::CapsuleColliderComponent>()) return false;

            return actor->RemoveComponent<Scene::CapsuleColliderComponent>();
        });
    }

    bool EditorCommands::ResetCapsuleCollider(Scene::Scene& scene, Core::UUID actorID)
    {
        return ApplyImmediateMutation(m_EditorController, scene, [&]()
        {
            Scene::Actor* actor = scene.FindActorByID(actorID);
            if (!actor) return false;

            auto* collider = actor->TryGetComponent<Scene::CapsuleColliderComponent>();
            if (!collider) return false;
            *collider = Scene::CapsuleColliderComponent{};

            return true;
        });
    }

    bool EditorCommands::PasteCapsuleColliderValues(Scene::Scene& scene, Core::UUID actorID, const Scene::CapsuleColliderComponent& collider)
    {
        if (!std::isfinite(collider.Radius) ||
            !std::isfinite(collider.Height) ||
            !std::isfinite(collider.Center.x) ||
            !std::isfinite(collider.Center.y) ||
            !std::isfinite(collider.Center.z))
        {
            return false;
        }

        constexpr float minimumDimension = 0.001f;
        constexpr float maximumHeight = 100000.0f;
        constexpr float maximumRadius = (maximumHeight - minimumDimension) * 0.5f;

        Scene::CapsuleColliderComponent validated = collider;
        validated.Radius = std::clamp(collider.Radius, minimumDimension, maximumRadius);

        const float minimumHeight = validated.Radius * 2.0f + minimumDimension;
        validated.Height = std::clamp(collider.Height, minimumHeight, maximumHeight);

        return ApplyImmediateMutation(m_EditorController, scene, [&]()
        {
            Scene::Actor* actor = scene.FindActorByID(actorID);
            if (!actor) return false;

            auto* existing = actor->TryGetComponent<Scene::CapsuleColliderComponent>();
            if (!existing) return false;
            *existing = validated;

            return true;
        });
    }


    bool EditorCommands::SetCapsuleColliderRadius(Scene::Scene& scene, Core::UUID actorID, const float radius)
    {
        if (!CanEditScene() || !std::isfinite(radius)) return false;

        Scene::Actor* actor = scene.FindActorByID(actorID);
        if (!actor) return false;

        auto* collider = actor->TryGetComponent<Scene::CapsuleColliderComponent>();
        if (!collider) return false;

        constexpr float minimumDimension = 0.001f;
        constexpr float maximumHeight = 100000.0f;
        constexpr float maximumRadius = (maximumHeight - minimumDimension) * 0.5f;

        collider->Radius = std::clamp(radius, minimumDimension, maximumRadius);
        const float minimumHeight = collider->Radius * 2.0f + minimumDimension;
        collider->Height = std::max(collider->Height, minimumHeight);

        return true;
    }

    bool EditorCommands::SetCapsuleColliderHeight(Scene::Scene& scene, Core::UUID actorID, const float height)
    {
        if (!CanEditScene() || !std::isfinite(height)) return false;

        Scene::Actor* actor = scene.FindActorByID(actorID);
        if (!actor) return false;

        auto* collider = actor->TryGetComponent<Scene::CapsuleColliderComponent>();
        if (!collider) return false;

        constexpr float minimumDimension = 0.001f;
        constexpr float maximumHeight = 100000.0f;
        const float minimumHeight = collider->Radius * 2.0f + minimumDimension;

        collider->Height = std::clamp(height, minimumHeight, maximumHeight);

        return true;
    }

    bool EditorCommands::SetCapsuleColliderCenter(Scene::Scene& scene, Core::UUID actorID, const glm::vec3& center)
    {
        if (!CanEditScene() ||
            !std::isfinite(center.x) ||
            !std::isfinite(center.y) ||
            !std::isfinite(center.z))
        {
            return false;
        }

        Scene::Actor* actor = scene.FindActorByID(actorID);
        if (!actor) return false;

        auto* collider = actor->TryGetComponent<Scene::CapsuleColliderComponent>();
        if (!collider) return false;
        collider->Center = center;

        return true;
    }

    #pragma endregion

#pragma endregion
}

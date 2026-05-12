#pragma once
#include <string>

#include "Quiet/Core/UUID.h"
#include "glm/vec3.hpp"
#include "glm/vec4.hpp"

namespace Quiet::Scene
{
    class Scene;
}

namespace Quiet::Project
{
    class ProjectContext;
}

namespace Quiet::Renderer::Vulkan
{
    class MeshLibrary;
}

namespace Quiet::Editor
{
    class EditorController;

    class EditorCommands
    {
    public:
        explicit EditorCommands(EditorController& editorController);

        [[nodiscard]] bool CanEditScene() const;

        void BeginInteraction(const Scene::Scene& scene);
        bool EndInteraction(const Scene::Scene& scene);

        bool CreateEmptyActor(Scene::Scene& scene, const std::string& name = "Actor");
        bool CreateStaticMeshActor(Scene::Scene& scene,
                                   const std::string& actorName,
                                   const std::string& meshResourceID,
                                   const Project::ProjectContext& projectContext,
                                   const Renderer::Vulkan::MeshLibrary& meshLibrary);
        bool CreateDirectionalLightActor(Scene::Scene& scene,
                                         const std::string& actorName = "Directional Light");
        bool DuplicateSelectedActor(Scene::Scene& scene);
        bool DeleteSelectedActor(Scene::Scene& scene);

        bool DuplicateActor(Scene::Scene& scene, Core::UUID actorID);
        bool DeleteActor(Scene::Scene& scene, Core::UUID actorID);

        bool RenameActor(Scene::Scene& scene, Core::UUID actorID, const std::string& newName);
        bool SetActorPosition(Scene::Scene& scene, Core::UUID actorID, const glm::vec3& position);
        bool SetActorRotation(Scene::Scene& scene, Core::UUID actorID, const glm::vec3& rotationDegrees);
        bool SetActorScale(Scene::Scene& scene, Core::UUID actorID, const glm::vec3& scale);

        bool AddStaticMesh(Scene::Scene& scene,
                           Core::UUID actorID,
                           const Project::ProjectContext& projectContext,
                           const Renderer::Vulkan::MeshLibrary& meshLibrary);

        bool RemoveStaticMesh(Scene::Scene& scene, Core::UUID actorID);

        bool SetStaticMeshResource(Scene::Scene& scene,
                                   Core::UUID actorID,
                                   const std::string& meshResourceID,
                                   const Project::ProjectContext& projectContext,
                                   const Renderer::Vulkan::MeshLibrary& meshLibrary);

        bool SetStaticMeshVisible(Scene::Scene& scene,
                                  Core::UUID actorID,
                                  bool visible);

        bool AddMaterial(Scene::Scene& scene, Core::UUID actorID, const glm::vec4& baseColor = glm::vec4{ 1.0f });
        bool RemoveMaterial(Scene::Scene& scene, Core::UUID actorID);
        bool SetMaterialBaseColor(Scene::Scene& scene, Core::UUID actorID, const glm::vec4& baseColor);
        bool SetMaterialBaseColorTextureAsset(Scene::Scene& scene, Core::UUID actorID, const std::string& textureAssetID);
        bool SetStaticMeshMaterialSlotBaseColor(Scene::Scene& scene, Core::UUID actorID, std::uint32_t slotIndex, const glm::vec4& baseColor);
        bool SetStaticMeshMaterialSlotBaseColorTextureAsset(Scene::Scene& scene, Core::UUID actorID, std::uint32_t slotIndex, const std::string& textureAssetID);

        bool AddDirectionalLight(Scene::Scene& scene, Core::UUID actorID);
        bool RemoveDirectionalLight(Scene::Scene& scene, Core::UUID actorID);
        bool SetDirectionalLightEnabled(Scene::Scene& scene, Core::UUID actorID, bool enabled);
        bool SetDirectionalLightDirection(Scene::Scene& scene, Core::UUID actorID, const glm::vec3& direction);
        bool SetDirectionalLightColor(Scene::Scene& scene, Core::UUID actorID, const glm::vec3& color);
        bool SetDirectionalLightIntensity(Scene::Scene& scene, Core::UUID actorID, float intensity);

        bool AddRotator(Scene::Scene& scene, Core::UUID actorID, float initialSpeed = 45.0f);
        bool RemoveRotator(Scene::Scene& scene, Core::UUID actorID);
        bool SetRotatorSpeed(Scene::Scene& scene, Core::UUID actorID, float speed);

        bool SetSceneLightDirection(Scene::Scene& scene, const glm::vec3& direction);
        bool SetSceneLightIntensity(Scene::Scene& scene, float intensity);
        bool SetSceneAmbientColor(Scene::Scene& scene, const glm::vec3& color);
        bool SetSceneAmbientIntensity(Scene::Scene& scene, float intensity);

    #pragma region Camera
        bool AddCamera(Scene::Scene& scene, Core::UUID actorID);
        bool RemoveCamera(Scene::Scene& scene, Core::UUID actorID);
        bool SetCameraPrimary(Scene::Scene& scene, Core::UUID actorID, bool primary);
        bool SetCameraFieldOfView(Scene::Scene& scene, Core::UUID actorID, float fovDegrees);
        bool SetCameraNearClip(Scene::Scene& scene, Core::UUID actorID, float nearClip);
        bool SetCameraFarClip(Scene::Scene& scene, Core::UUID actorID, float farClip);
    #pragma endregion

    private:
        EditorController& m_EditorController;
    };
}
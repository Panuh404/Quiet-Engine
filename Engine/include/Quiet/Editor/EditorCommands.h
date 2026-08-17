#pragma once

#include <cstddef>
#include <string>
#include <vector>
#include <cstdint>

#include "Quiet/Core/UUID.h"
#include "glm/vec3.hpp"
#include "glm/vec4.hpp"
#include "Scene/SceneHierarchy.h"

namespace Quiet::Scene
{
    class Scene;
    enum class RigidBodyMotionType : std::uint8_t;

    struct CameraComponent;
    struct DirectionalLightComponent;
    struct MaterialComponent;
    struct RotatorComponent;
    struct TransformComponent;
    struct RigidBodyComponent;
    struct BoxColliderComponent;
    struct SphereColliderComponent;
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

    #pragma region Actors
        bool CreateEmptyActor(Scene::Scene& scene, const std::string& name = "Actor");
        bool DuplicateActor(Scene::Scene& scene, Core::UUID actorID);
        bool DeleteActor(Scene::Scene& scene, Core::UUID actorID);
        bool RenameActor(Scene::Scene& scene, Core::UUID actorID, const std::string& newName);

        bool DuplicateSelectedActor(Scene::Scene& scene);
        bool DeleteSelectedActor(Scene::Scene& scene);

        bool SetActorPosition(Scene::Scene& scene, Core::UUID actorID, const glm::vec3& position);
        bool SetActorRotation(Scene::Scene& scene, Core::UUID actorID, const glm::vec3& rotationDegrees);
        bool SetActorScale(Scene::Scene& scene, Core::UUID actorID, const glm::vec3& scale);
        bool ResetActorTransform(Scene::Scene& scene, Core::UUID actorID);
        bool PasteActorTransformValues(Scene::Scene& scene, Core::UUID actorID, const Scene::TransformComponent& transform);
    #pragma endregion

    #pragma region Folders
        bool CreateHierarchyFolder(Scene::Scene& scene, const std::string& name = "Folder");
        bool RenameHierarchyFolder(Scene::Scene& scene, Core::UUID folderID, const std::string& newName);
        bool DeleteEmptyHierarchyFolder(Scene::Scene& scene, Core::UUID folderID);
        bool MoveActorToHierarchyFolder(Scene::Scene& scene, Core::UUID actorID, Core::UUID folderID);
        bool MoveActorToHierarchyRoot(Scene::Scene& scene, Core::UUID actorID);
        bool MoveHierarchyRootNode(Scene::Scene& scene, Scene::SceneHierarchyNodeType nodeType, Core::UUID nodeID, std::size_t destinationIndex);
    #pragma endregion

    #pragma region Static Meshes
        bool CreateStaticMeshActor(Scene::Scene& scene, const std::string& actorName, const std::string& meshResourceID, const Project::ProjectContext& projectContext,
                                   const Renderer::Vulkan::MeshLibrary& meshLibrary, const glm::vec3& position = glm::vec3(0.0f));
        bool AddStaticMesh(Scene::Scene& scene, Core::UUID actorID, const Project::ProjectContext& projectContext, const Renderer::Vulkan::MeshLibrary& meshLibrary);
        bool RemoveStaticMesh(Scene::Scene& scene, Core::UUID actorID);
        bool PasteStaticMeshValues(Scene::Scene& scene, Core::UUID actorID, const std::string& meshResourceID, bool visible, const Project::ProjectContext& projectContext, const Renderer::Vulkan::MeshLibrary& meshLibrary);
        bool SetStaticMeshResource(Scene::Scene& scene, Core::UUID actorID, const std::string& meshResourceID, const Project::ProjectContext& projectContext, const Renderer::Vulkan::MeshLibrary& meshLibrary);
        bool SetStaticMeshVisible(Scene::Scene& scene, Core::UUID actorID, bool visible);
    #pragma endregion

    #pragma region Materials
        bool AddMaterial(Scene::Scene& scene, Core::UUID actorID, const glm::vec4& baseColor = glm::vec4{ 1.0f });
        bool RemoveMaterial(Scene::Scene& scene, Core::UUID actorID);
        bool ResetMaterial(Scene::Scene& scene, Core::UUID actorID);
        bool PasteMaterialValues(Scene::Scene& scene, Core::UUID actorID, const Scene::MaterialComponent& material, const std::vector<Scene::MaterialComponent>& materialSlots, const Project::ProjectContext& projectContext);
        bool SetMaterialBaseColor(Scene::Scene& scene, Core::UUID actorID, const glm::vec4& baseColor);
        bool SetMaterialBaseColorTextureAsset(Scene::Scene& scene, Core::UUID actorID, const std::string& textureAssetID);
        bool SetStaticMeshMaterialSlotBaseColor(Scene::Scene& scene, Core::UUID actorID, std::uint32_t slotIndex, const glm::vec4& baseColor);
        bool SetStaticMeshMaterialSlotBaseColorTextureAsset(Scene::Scene& scene, Core::UUID actorID, std::uint32_t slotIndex, const std::string& textureAssetID);
    #pragma endregion

    #pragma region Directional Lights
        bool CreateDirectionalLightActor(Scene::Scene& scene, const std::string& actorName = "Directional Light");
        bool AddDirectionalLight(Scene::Scene& scene, Core::UUID actorID);
        bool RemoveDirectionalLight(Scene::Scene& scene, Core::UUID actorID);
        bool ResetDirectionalLight(Scene::Scene& scene, Core::UUID actorID);
        bool PasteDirectionalLightValues(Scene::Scene& scene, Core::UUID actorID, const Scene::DirectionalLightComponent& directionalLight);

        bool SetDirectionalLightEnabled(Scene::Scene& scene, Core::UUID actorID, bool enabled);
        bool SetDirectionalLightDirection(Scene::Scene& scene, Core::UUID actorID, const glm::vec3& direction);
        bool SetDirectionalLightColor(Scene::Scene& scene, Core::UUID actorID, const glm::vec3& color);
        bool SetDirectionalLightIntensity(Scene::Scene& scene, Core::UUID actorID, float intensity);
    #pragma endregion

    #pragma region Rotator
        bool AddRotator(Scene::Scene& scene, Core::UUID actorID, float initialSpeed = 45.0f);
        bool RemoveRotator(Scene::Scene& scene, Core::UUID actorID);
        bool ResetRotator(Scene::Scene& scene, Core::UUID actorID);
        bool PasteRotatorValues(Scene::Scene& scene, Core::UUID actorID, const Scene::RotatorComponent& rotator);
        bool SetRotatorSpeed(Scene::Scene& scene, Core::UUID actorID, float speed);
    #pragma endregion

    #pragma region Scene Lighting
        bool SetSceneLightDirection(Scene::Scene& scene, const glm::vec3& direction);
        bool SetSceneLightIntensity(Scene::Scene& scene, float intensity);
        bool SetSceneAmbientColor(Scene::Scene& scene, const glm::vec3& color);
        bool SetSceneAmbientIntensity(Scene::Scene& scene, float intensity);
    #pragma endregion

    #pragma region Camera
        bool AddCamera(Scene::Scene& scene, Core::UUID actorID);
        bool RemoveCamera(Scene::Scene& scene, Core::UUID actorID);
        bool ResetCamera(Scene::Scene& scene, Core::UUID actorID);
        bool PasteCameraValues(Scene::Scene& scene, Core::UUID actorID, const Scene::CameraComponent& camera);

        bool SetCameraPrimary(Scene::Scene& scene, Core::UUID actorID, bool primary);
        bool SetCameraFieldOfView(Scene::Scene& scene, Core::UUID actorID, float fovDegrees);
        bool SetCameraNearClip(Scene::Scene& scene, Core::UUID actorID, float nearClip);
        bool SetCameraFarClip(Scene::Scene& scene, Core::UUID actorID, float farClip);
    #pragma endregion

    #pragma region Physics

        // RIGID BODY
        bool AddRigidBody(Scene::Scene& scene, Core::UUID actorID);
        bool RemoveRigidBody(Scene::Scene&scene, Core::UUID actorID);
        bool ResetRigidBody(Scene::Scene& scene, Core::UUID actorID);
        bool PasteRigidBodyValues(Scene::Scene& scene, Core::UUID actorID, const Scene::RigidBodyComponent& rigidBody);

        bool SetRigidBodyMotionType(Scene::Scene& scene, Core::UUID actorID, Scene::RigidBodyMotionType motionType);
        bool SetRigidBodyFriction(Scene::Scene& scene, Core::UUID actorID, float friction);
        bool SetRigidBodyRestitution(Scene::Scene& scene, Core::UUID actorID, float restitution);
        bool SetRigidBodyLinearDamping(Scene::Scene& scene, Core::UUID actorID, float damping);
        bool SetRigidBodyAngularDamping(Scene::Scene& scene, Core::UUID actorID, float damping);
        bool SetRigidBodyGravityFactor(Scene::Scene& scene, Core::UUID actorID, float gravityFactor);
        bool SetRigidBodyAllowSleeping(Scene::Scene& scene, Core::UUID actorID, bool allowSleeping);

        // BOX COLLIDER
        bool AddBoxCollider(Scene::Scene& scene, Core::UUID actorID);
        bool RemoveBoxCollider(Scene::Scene& scene, Core::UUID actorID);
        bool ResetBoxCollider(Scene::Scene& scene, Core::UUID actorID);
        bool PasteBoxColliderValues(Scene::Scene& scene, Core::UUID actorID, const Scene::BoxColliderComponent& collider);

        bool SetBoxColliderSize(Scene::Scene& scene, Core::UUID actorID, const glm::vec3& size);
        bool SetBoxColliderCenter(Scene::Scene& scene, Core::UUID actorID, const glm::vec3& center);

        // SPHERE COLLIDER
        bool AddSphereCollider(Scene::Scene& scene, Core::UUID actorID);
        bool RemoveSphereCollider(Scene::Scene& scene, Core::UUID actorID);
        bool ResetSphereCollider(Scene::Scene& scene, Core::UUID actorID);
        bool PasteSphereColliderValues(Scene::Scene& scene, Core::UUID actorID, const Scene::SphereColliderComponent& collider);

        bool SetSphereColliderRadius(Scene::Scene& scene, Core::UUID actorID, float radius);
        bool SetSphereColliderCenter(Scene::Scene& scene, Core::UUID actorID, const glm::vec3& center);

    #pragma endregion

    private:
        EditorController& m_EditorController;
    };
}
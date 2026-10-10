#pragma once

#include <cstddef>
#include <string>
#include <vector>
#include <cstdint>

#include "Quiet/Core/UUID.h"
#include "glm/vec3.hpp"
#include "glm/vec4.hpp"
#include "Scene/SceneHierarchy.h"

namespace Quiet::Audio
{
    enum class AudioBus : std::uint8_t;
}

namespace Quiet::Scene
{
    class Scene;
    enum class RigidBodyMotionType : std::uint8_t;

    struct AudioListenerComponent;
    struct AudioSourceComponent;
    struct CameraComponent;
    struct DirectionalLightComponent;
    struct MaterialComponent;
    struct RotatorComponent;
    struct TransformComponent;
    struct ScriptComponent;
    struct ScriptProperty;
    struct RigidBodyComponent;
    struct BoxColliderComponent;
    struct SphereColliderComponent;
    struct CapsuleColliderComponent;
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

    /**
     * @brief Provides Scene authoring operations through an Editor controller.
     * @ingroup quiet_editor
     *
     * Borrows the controller. Individual operations receive their target Scene.
     * Some operations record immediate mutations; others require an enclosing
     * interaction boundary for history tracking.
     */
    class EditorCommands
    {
    public:
        explicit EditorCommands(EditorController& editorController);

        /**
         * @brief Reports whether Editor state currently selects Edit mode.
         */
        [[nodiscard]] bool CanEditScene() const;

        /**
         * @brief Begins a Scene history boundary when editing is permitted.
         * @note An existing pending boundary retains its original snapshot.
         */
        void BeginInteraction(const Scene::Scene& scene);

        /**
         * @brief Ends a Scene history boundary when editing is permitted.
         * @return True when history records a differing final snapshot.
         * @note A false result does not undo Scene changes.
         */
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

    #pragma region Scene Audio

        bool SetSceneMasterVolume(Scene::Scene& scene, float volume);
        bool SetSceneMusicVolume(Scene::Scene& scene, float volume);
        bool SetSceneSFXVolume(Scene::Scene& scene, float volume);
        bool SetSceneUIVolume(Scene::Scene& scene, float volume);

    #pragma endregion

    #pragma region Scene Lighting

        bool SetSceneLightDirection(Scene::Scene& scene, const glm::vec3& direction);
        bool SetSceneLightIntensity(Scene::Scene& scene, float intensity);
        bool SetSceneAmbientColor(Scene::Scene& scene, const glm::vec3& color);
        bool SetSceneAmbientIntensity(Scene::Scene& scene, float intensity);

    #pragma endregion

    #pragma region Scene Physics

        bool SetSceneGravity(Scene::Scene& scene, const glm::vec3& gravity);

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

    #pragma region Scripting

        bool AddScript(Scene::Scene& scene, Core::UUID actorID);
        bool RemoveScript(Scene::Scene& scene, Core::UUID actorID);
        bool ResetScript(Scene::Scene& scene, Core::UUID actorID);
        bool PasteScriptValues(Scene::Scene& scene, Core::UUID actorID, const Scene::ScriptComponent& script, const Project::ProjectContext& projectContext);

        bool SetScriptAsset(Scene::Scene& scene, Core::UUID actorID, const std::string& scriptAssetID, const Project::ProjectContext& projectContext);

        bool RefreshScriptProperties(Scene::Scene& scene, Core::UUID actorID, const Project::ProjectContext& projectContext);
        bool SetScriptPropertyValue(Scene::Scene& scene, Core::UUID actorID, const Scene::ScriptProperty& property);
        bool SetScriptPropertyValueImmediate(Scene::Scene& scene, Core::UUID actorID, const Scene::ScriptProperty& property, const Project::ProjectContext& projectContext);
    #pragma endregion

    #pragma region Audio

        // AUDIO SOURCE
        bool AddAudioSource(Scene::Scene& scene, Core::UUID actorID);
        bool RemoveAudioSource(Scene::Scene& scene, Core::UUID actorID);
        bool ResetAudioSource(Scene::Scene& scene, Core::UUID actorID);
        bool PasteAudioSourceValues(Scene::Scene& scene, Core::UUID actorID, const Scene::AudioSourceComponent& audioSource, const Project::ProjectContext& projectContext);

        bool SetAudioSourceAsset(Scene::Scene& scene, Core::UUID actorID, const std::string& audioAssetID, const Project::ProjectContext& projectContext);
        bool SetAudioSourceBus(Scene::Scene& scene, Core::UUID actorID, Audio::AudioBus bus);
        bool SetAudioSourceVolume(Scene::Scene& scene, Core::UUID actorID, float volume);
        bool SetAudioSourcePitch(Scene::Scene& scene, Core::UUID actorID, float pitch);
        bool SetAudioSourcePlayOnStart(Scene::Scene& scene, Core::UUID actorID, bool playOnStart);
        bool SetAudioSourceLoop(Scene::Scene& scene, Core::UUID actorID, bool loop);
        bool SetAudioSourceSpatial(Scene::Scene& scene, Core::UUID actorID, bool spatial);
        bool SetAudioSourceMinDistance(Scene::Scene& scene, Core::UUID actorID, float minDistance);
        bool SetAudioSourceMaxDistance(Scene::Scene& scene, Core::UUID actorID, float maxDistance);
        bool SetAudioSourceRolloff(Scene::Scene& scene, Core::UUID actorID, float rolloff);
        bool SetAudioSourceConeInnerAngle(Scene::Scene& scene, Core::UUID actorID, float angleDegrees);
        bool SetAudioSourceConeOuterAngle(Scene::Scene& scene, Core::UUID actorID, float angleDegrees);
        bool SetAudioSourceConeOuterGain(Scene::Scene& scene, Core::UUID actorID, float gain);
        bool SetAudioSourceDopplerFactor(Scene::Scene& scene, Core::UUID actorID, float dopplerFactor);

        // AUDIO LISTENER
        bool AddAudioListener(Scene::Scene& scene, Core::UUID actorID);
        bool RemoveAudioListener(Scene::Scene& scene, Core::UUID actorID);
        bool ResetAudioListener(Scene::Scene& scene, Core::UUID actorID);
        bool PasteAudioListenerValues(Scene::Scene& scene, Core::UUID actorID, const Scene::AudioListenerComponent& audioListener);

        bool SetAudioListenerPrimary(Scene::Scene& scene, Core::UUID actorID, bool primary);

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

        // CAPSULE COLLIDER
        bool AddCapsuleCollider(Scene::Scene& scene, Core::UUID actorID);
        bool RemoveCapsuleCollider(Scene::Scene& scene, Core::UUID actorID);
        bool ResetCapsuleCollider(Scene::Scene& scene, Core::UUID actorID);
        bool PasteCapsuleColliderValues(Scene::Scene& scene, Core::UUID actorID, const Scene::CapsuleColliderComponent& collider);

        bool SetCapsuleColliderRadius(Scene::Scene& scene, Core::UUID actorID, float radius);
        bool SetCapsuleColliderHeight(Scene::Scene& scene, Core::UUID actorID, float height);
        bool SetCapsuleColliderCenter(Scene::Scene& scene, Core::UUID actorID, const glm::vec3& center);

    #pragma endregion

    private:
        EditorController& m_EditorController;
    };
}
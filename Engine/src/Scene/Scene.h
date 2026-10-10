#pragma once

#include <cstddef>
#include <string>
#include <vector>
#include <memory>

#include <glm/vec3.hpp>

#include "Scene/Actor.h"
#include "Scene/SceneHierarchy.h"
#include "Scene/SceneAudioSettings.h"
#include "Scene/ScenePhysicsSettings.h"
#include "Scene/SceneRenderSettings.h"
#include "Physics/PhysicsContactEvent.h"

namespace Quiet::Project
{
    class ProjectContext;
}

namespace Quiet::Audio
{
    enum class AudioPlaybackState;

    class AudioSubsystem;
    class AudioWorld;
}

namespace Quiet::Input
{
    class Input;
}

namespace Quiet::Physics
{
    class PhysicsSubsystem;
    class PhysicsWorld;
}

namespace Quiet::Scripting
{
    class ScriptWorld;
}

namespace Quiet::Scene
{
    /**
     * @brief Selects inactive, Simulate, or Play Scene runtime behavior.
     * @ingroup quiet_scene
     */
    enum class RuntimeMode
    {
        None,     ///< Scene runtime input and updates are inactive.
        Simulate, ///< Runs physics, audio, and scripts with Simulate input behavior.
        Play      ///< Runs physics, audio, and scripts with Play camera input.
    };

    /**
     * @brief Owns scene entities, components, Actor handles, and runtime state.
     * @ingroup quiet_scene
     *
     * Components reside in the EnTT registry. Actor handles reside in a
     * separate vector and refer to entities in that registry.
     *
     * @note Creation may invalidate Actor pointers and references through
     *       vector reallocation. Destruction invalidates those referring
     *       to the erased element and subsequent elements.
     */

    class Scene
    {
    public:
        Scene();

        /**
         * @brief Creates an inactive Scene copy with preserved Actor UUIDs.
         * @param other Source Scene.
         * @note Copies supported component values, hierarchy, and scene settings.
         *       Runtime worlds and pending requests are not copied.
         * @note StaticMeshComponent mesh resources retain shared ownership.
         */
        Scene(const Scene& other);

        /**
         * @brief Replaces this Scene with an inactive copy of another Scene.
         * @param other Source Scene.
         * @return This Scene.
         * @note Self-assignment leaves the Scene unchanged.
         * @note Otherwise, clears the destination before copying, invalidating
         *       its previous Actor handles and component references.
         * @note Preserves Actor UUIDs and copies supported component values,
         *       hierarchy, and scene settings. Mesh resources remain shared.
         * @note Copy failure does not restore the previous destination state.
         */
        Scene& operator=(const Scene& other);

        ~Scene();

        /**
         * @brief Creates an Actor with a generated UUID.
         * @param name Initial Actor name.
         * @return A Scene-owned handle with identity and Transform components.
         * @note Appending the handle may invalidate existing Actor pointers
         *       and references through vector reallocation.
         */
        Actor& CreateActor(const std::string& name);

        /**
         * @brief Creates an Actor using a caller-supplied UUID.
         * @param id Valid UUID not already used by an Actor in this Scene.
         * @param name Initial Actor name.
         * @pre id is valid and unused; this function does not check these conditions.
         * @return A Scene-owned handle with identity and Transform components.
         * @note Appending the handle may invalidate existing Actor pointers
         *       and references through vector reallocation.
         */
        Actor& CreateActorWithID(Core::UUID id, const std::string& name);

        /**
         * @brief Immediately duplicates an Actor with a newly generated UUID.
         * @param sourceID UUID of the source Actor in this Scene.
         * @return A Scene-owned handle to the duplicate.
         * @throws std::runtime_error If the duplication operation returns nullptr.
         * @note Copies supported component values and appends the duplicate
         *       to the hierarchy root. Script Actor references are not remapped.
         * @note Creation may invalidate existing Actor pointers and references
         *       through vector reallocation.
         */
        Actor& DuplicateActor(Core::UUID sourceID);

        /**
         * @brief Attempts immediate Actor duplication with a newly generated UUID.
         * @param sourceID UUID of the source Actor in this Scene.
         * @return A Scene-owned duplicate handle, or nullptr if the source is
         *         absent or the generated destination UUID is invalid or used.
         * @note Copies supported component values and appends the duplicate
         *       to the hierarchy root. Script Actor references are not remapped.
         * @note Creation may invalidate existing Actor pointers and references
         *       through vector reallocation.
         * @note Allocation and component-copy exceptions are not caught.
         */
        Actor* TryDuplicateActor(Core::UUID sourceID);

        bool DestroyActorByID(Core::UUID id);
        void Clear();

        void ResetHierarchyFromActors();
        void SetHierarchy(SceneHierarchyState hierarchy);

        Core::UUID CreateHierarchyFolder(const std::string& name = "Folder");

        [[nodiscard]] SceneHierarchyFolder* FindHierarchyFolder(Core::UUID folderID);
        [[nodiscard]] const SceneHierarchyFolder* FindHierarchyFolder(Core::UUID folderID) const;

        bool RenameHierarchyFolder(Core::UUID folderID, const std::string& newName);
        bool DeleteEmptyHierarchyFolder(Core::UUID folderID);

        bool MoveActorToHierarchyFolder(Core::UUID actorID, Core::UUID folderID);
        bool MoveActorToHierarchyRoot(Core::UUID actorID);
        bool MoveHierarchyRootNode(SceneHierarchyNodeType nodeType, Core::UUID nodeID, std::size_t destinationIndex);

        [[nodiscard]] SceneHierarchyState& GetHierarchy() { return m_Hierarchy; }
        [[nodiscard]] const SceneHierarchyState& GetHierarchy() const { return m_Hierarchy; }

        /**
         * @brief Starts Scene physics, audio, and scripting for the requested mode.
         * @param mode Simulate or Play; None logs a warning and leaves state unchanged.
         * @param physicsSubsystem Application-owned physics backend resources.
         * @param audioSubsystem Application-owned audio backend resources.
         * @param projectContext Project context borrowed by ScriptWorld.
         * @param input Input object borrowed by ScriptWorld.
         * @pre Supplied dependencies remain alive while runtime worlds use them.
         * @note Ends an existing active runtime before starting the requested mode.
         * @note Native worlds are constructed before script instances begin.
         * @note Startup exceptions propagate without transactional rollback.
         */
        void BeginRuntime(RuntimeMode mode,
                          Physics::PhysicsSubsystem& physicsSubsystem,
                          Audio::AudioSubsystem& audioSubsystem,
                          const Project::ProjectContext& projectContext,
                          const Input::Input& input);

        /**
         * @brief Ends active runtime while retaining Scene component data.
         * @note Does nothing when runtime mode is None.
         * @note Ends scripts before releasing scripting, audio, and physics worlds.
         * @note Clears pending Actor requests and contact events.
         * @note Does not restore component values to their pre-runtime state.
         */
        void EndRuntime();

        /**
         * @brief Applies the active runtime mode's Scene input behavior.
         * @param input Current input state.
         * @param deltaTime Frame duration in seconds.
         * @note Does nothing while runtime is inactive.
         * @note Play updates the primary camera when present; Simulate currently
         *       performs no Scene-level input work.
         */
        void HandleRuntimeInput(const Input::Input& input, float deltaTime);

        /**
         * @brief Updates active Scene runtime using the supplied frame duration.
         * @param deltaTime Frame duration in seconds.
         * @note Does nothing while runtime is inactive.
         * @note Physics uses its own fixed-step accumulator.
         * @note Flushing Actor requests can invalidate Actor pointers and references.
         */
        void Update(float deltaTime);

        // Runtime Actor API
        Core::UUID RequestRuntimeActorCreate(const std::string& name);
        Core::UUID RequestRuntimeActorDuplicate(Core::UUID sourceActorID);
        bool RequestRuntimeActorDestroy(Core::UUID actorID);

        // Physics Runtime API
        bool ApplyPhysicsImpulse(Core::UUID actorID, const glm::vec3& impulse);
        bool SetPhysicsLinearVelocity(Core::UUID actorID, const glm::vec3& velocity);
        [[nodiscard]] const std::vector<Physics::PhysicsContactEvent>& GetPhysicsContactEvents() const { return m_PhysicsContactEvents;}

        // Audio Runtime API
        bool PlayAudioSource(Core::UUID actorID);
        bool StopAudioSource(Core::UUID actorID);
        bool PauseAudioSource(Core::UUID actorID);
        bool ResumeAudioSource(Core::UUID actorID);
        bool SeekAudioSource(Core::UUID actorID, float seconds);
        [[nodiscard]] Audio::AudioPlaybackState GetAudioSourcePlaybackState(Core::UUID actorID) const;

        [[nodiscard]] bool IsRuntimeActive() const { return m_RuntimeMode != RuntimeMode::None; }
        [[nodiscard]] RuntimeMode GetRuntimeMode() const { return m_RuntimeMode; }

        [[nodiscard]] std::vector<Actor>& GetActors() { return m_Actors; }
        [[nodiscard]] const std::vector<Actor>& GetActors() const { return m_Actors; }
        [[nodiscard]] const entt::registry& GetRegistry() const { return m_Registry; }

        [[nodiscard]] SceneAudioSettings& GetAudioSettings() { return m_AudioSettings; }
        [[nodiscard]] const SceneAudioSettings& GetAudioSettings() const { return m_AudioSettings; }

        [[nodiscard]] ScenePhysicsSettings& GetPhysicsSettings() { return m_PhysicsSettings; }
        [[nodiscard]] const ScenePhysicsSettings& GetPhysicsSettings() const { return m_PhysicsSettings; }

        [[nodiscard]] SceneRenderSettings& GetRenderSettings() { return m_RenderSettings; }
        [[nodiscard]] const SceneRenderSettings& GetRenderSettings() const { return m_RenderSettings; }

        [[nodiscard]] Actor* FindActorByID(Core::UUID id);
        [[nodiscard]] const Actor* FindActorByID(Core::UUID id) const;
        [[nodiscard]] entt::entity FindEntityByID(Core::UUID id) const;

        [[nodiscard]] Actor* FindPrimaryCameraActor();
        [[nodiscard]] const Actor* FindPrimaryCameraActor() const;

        [[nodiscard]] Actor* FindPrimaryAudioListenerActor();
        [[nodiscard]] const Actor* FindPrimaryAudioListenerActor() const;

        [[nodiscard]] const DirectionalLightComponent* FindActiveDirectionalLight() const;

    private:
        void AddActorToHierarchyRoot(Core::UUID actorID);
        void RemoveActorFromHierarchy(Core::UUID actorID);
        void FlushRuntimeActorCreations();
        void FlushRuntimeActorDestructions();
        void SanitizeHierarchy();

        Actor* TryDuplicateActorWithID(Core::UUID sourceID, Core::UUID duplicateID);

        void OnRuntimeBegin(RuntimeMode mode);
        void OnRuntimeEnd(RuntimeMode mode);
        void OnSimulateBegin();
        void OnSimulateEnd();
        void OnPlayBegin();
        void OnPlayEnd();

        void HandleSimulateInput(const Input::Input& input, float deltaTime);
        void HandlePlayInput(const Input::Input& input, float deltaTime);
        void UpdateRuntime(float deltaTime);

    private:
        entt::registry m_Registry;
        std::vector<Actor> m_Actors;
        SceneHierarchyState m_Hierarchy{};

        SceneAudioSettings m_AudioSettings{};
        ScenePhysicsSettings m_PhysicsSettings{};
        SceneRenderSettings m_RenderSettings{};
        RuntimeMode m_RuntimeMode = RuntimeMode::None;

        std::unique_ptr<Physics::PhysicsWorld> m_PhysicsWorld;
        std::unique_ptr<Audio::AudioWorld> m_AudioWorld;
        std::unique_ptr<Scripting::ScriptWorld> m_ScriptWorld;

        struct RuntimeActorCreationRequest
        {
            Core::UUID ActorID{};
            Core::UUID SourceActorID{};
            std::string Name;
        };

        std::vector<RuntimeActorCreationRequest> m_PendingRuntimeActorCreations;
        std::vector<Core::UUID> m_PendingRuntimeActorDestructions;
        std::vector<Physics::PhysicsContactEvent> m_PhysicsContactEvents;
    };
}
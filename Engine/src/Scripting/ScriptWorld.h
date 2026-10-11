#pragma once

#include <cstddef>
#include <memory>
#include <filesystem>
#include <string_view>

#include "Quiet/Core/UUID.h"

namespace Quiet::Input
{
    class Input;
}

namespace Quiet::Project
{
    class ProjectContext;
}

namespace Quiet::Scene
{
    class Actor;
    class Scene;
}

namespace Quiet::Scripting
{
    /**
     * @brief Owns one Scene's Lua state and runtime script instances.
     * @ingroup quiet_scripting
     *
     * Borrows Scene, ProjectContext, and Input, which must remain valid
     * while the world uses them. Actor instances use individual environments
     * within the world's shared Lua state.
     */
    class ScriptWorld
    {
    public:
        ScriptWorld(Scene::Scene& scene,
                    const Project::ProjectContext& projectContext,
                    const Input::Input& input);
        ~ScriptWorld();

        ScriptWorld(const ScriptWorld&) = delete;
        ScriptWorld& operator=(const ScriptWorld&) = delete;
        ScriptWorld(ScriptWorld&&) = delete;
        ScriptWorld& operator=(ScriptWorld&&) = delete;

        /**
         * @brief Builds initial instances from Actors with assigned scripts.
         * @note Clears existing instances and failed-instance records.
         * @note Refuses rebuilding after scripting has begun.
         * @note Failed candidates are skipped; no aggregate success is returned.
         */
        void BuildInstances();

        /**
         * @brief Begins scripting and starts the existing instances.
         * @note Marks the world begun before invoking instance OnCreate callbacks.
         */
        void Begin();

        /**
         * @brief Synchronizes instances, checks source changes, and runs OnUpdate.
         * @param deltaTime Elapsed seconds; zero is accepted.
         * @note Synchronization and reload checks precede delta validation.
         * @note Nonfinite or negative deltas skip OnUpdate callbacks.
         * @note Protected callback failures fault the affected instance.
         */
        void Update(float deltaTime);

        /**
         * @brief Delivers buffered Scene contacts to optional collision callbacks.
         * @note Requires started, non-faulted instances and an existing other Actor.
         * @note Protected callback failures fault the receiving instance.
         */
        void DispatchPhysicsContactEvents();

        /**
         * @brief Removes an Actor's instance before Actor destruction.
         * @note Invokes OnDestroy only for a started instance with that callback.
         */
        void NotifyActorWillBeDestroyed(Core::UUID actorID);

        /**
         * @brief Destroys instances in reverse order and resets the begun state.
         * @note Started instances may receive OnDestroy even when faulted.
         */
        void End();

        /**
         * @brief Reports whether Lua world initialization completed.
         * @note Does not establish successful instance loading or begun callbacks.
         */
        [[nodiscard]] bool IsInitialized() const { return m_Initialized; }

    private:
        struct RuntimeState;

        void Initialize();
        void Shutdown();

        bool ResolveScriptPath(std::string_view scriptAssetID, std::filesystem::path& outPath, std::string& outError) const;

        bool CreateInstance(const Scene::Actor& actor, bool recordFailure = true);
        void StartInstance(std::size_t index);
        void DestroyInstance(std::size_t index);

        /**
         * @brief Removes stale instances and starts newly assigned scripts.
         * @note Matching failed-creation records suppress further creation attempts.
         * @note Property edits alone do not reinstall existing instance bindings.
         */
        void SynchronizeInstances();

        /**
         * @brief Attempts replacement when recorded source timestamps change.
         * @note Failed construction retains the old instance and records the attempt.
         * @note Successful construction ends the old instance before new OnCreate.
         * @note A failing replacement OnCreate does not restore the old instance.
         * @note Does not migrate arbitrary Lua environment state.
         */
        void ReloadChangedInstances();

    private:
        Scene::Scene& m_Scene;
        const Project::ProjectContext& m_ProjectContext;
        const Input::Input& m_Input;

        std::unique_ptr<RuntimeState> m_State;

        bool m_Initialized = false;
        bool m_Begun = false;
    };
}
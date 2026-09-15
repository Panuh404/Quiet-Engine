#include "Scripting/ScriptWorld.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <sol/sol.hpp>

#include "Quiet/Core/Log.h"
#include "Quiet/Core/UUID.h"
#include "Quiet/Project/AssetRegistry.h"
#include "Quiet/Project/AssetType.h"
#include "Quiet/Project/ProjectContext.h"
#include "Scene/Components/ScriptComponent.h"
#include "Scripting/Bindings/Bindings.h"
#include "Physics/PhysicsContactEvent.h"
#include "Scene/Scene.h"

namespace Quiet::Scripting
{
    struct ScriptWorld::RuntimeState
    {
        struct ScriptInstance
        {
            ScriptInstance(
                const Core::UUID actorID,
                std::string scriptAssetID,
                std::filesystem::path sourcePath,
                const std::filesystem::file_time_type sourceWriteTime,
                sol::environment environment)
                : ActorID(actorID),
                  ScriptAssetID(std::move(scriptAssetID)),
                  SourcePath(std::move(sourcePath)),
                  SourceWriteTime(sourceWriteTime),
                  Environment(std::move(environment))
            {}

            Core::UUID ActorID{};
            std::string ScriptAssetID;

            std::filesystem::path SourcePath;
            std::filesystem::file_time_type SourceWriteTime{};

            sol::environment Environment;

            std::optional<sol::protected_function> OnCreate;
            std::optional<sol::protected_function> OnUpdate;
            std::optional<sol::protected_function> OnDestroy;

            std::optional<sol::protected_function> OnCollisionEnter;
            std::optional<sol::protected_function> OnCollisionExit;

            bool Started = false;
            bool Faulted = false;
        };

        sol::state Lua;
        std::vector<ScriptInstance> Instances;
        std::unordered_map<Core::UUID, std::string> FailedInstances;
    };

    ScriptWorld::ScriptWorld(Scene::Scene& scene, const Project::ProjectContext& projectContext, const Input::Input& input)
        : m_Scene(scene), m_ProjectContext(projectContext), m_Input(input)
    {
        Initialize();
    }

    ScriptWorld::~ScriptWorld()
    {
        Shutdown();
    }

    void ScriptWorld::Initialize()
    {
        if (m_Initialized) return;
        m_State = std::make_unique<RuntimeState>();

        try
        {
            m_State->Lua.open_libraries(
                sol::lib::base,
                sol::lib::coroutine,
                sol::lib::string,
                sol::lib::math,
                sol::lib::table,
                sol::lib::utf8
            );

            const sol::protected_function_result versionResult = m_State->Lua.safe_script("return _VERSION", sol::script_pass_on_error);
            if (!versionResult.valid())
            {
                const sol::error error = versionResult;
                throw std::runtime_error(std::string("Lua runtime validation failed: ") + error.what());
            }

            const std::string version = versionResult.get<std::string>();
            m_Initialized = true;
            QUIET_CORE_INFO("Initialized scripting world. runtime={}", version);
        }
        catch (...)
        {
            m_State.reset();
            throw;
        }
    }

    void ScriptWorld::BuildInstances()
    {
        if (!m_Initialized || !m_State) return;
        if (m_Begun)
        {
            QUIET_CORE_WARN("Cannot rebuild runtime script instances after scripting has begun.");
            return;
        }

        m_State->Instances.clear();
        m_State->FailedInstances.clear();

        std::size_t loadedCount = 0;
        std::size_t skippedCount = 0;

        for (const Scene::Actor& actor : m_Scene.GetActors())
        {
            const auto* script = actor.TryGetComponent<Scene::ScriptComponent>();
            if (!script || script->ScriptAssetID.empty()) continue;

            if (CreateInstance(actor))
                ++loadedCount;
            else
                ++skippedCount;
        }

        QUIET_CORE_INFO("Built runtime script instances. loaded={}, skipped={}", loadedCount, skippedCount);
    }

    void ScriptWorld::Begin()
    {
        if (!m_Initialized || !m_State || m_Begun) return;
        m_Begun = true;

        for (std::size_t index = 0; index < m_State->Instances.size(); ++index)
        {
            StartInstance(index);
        }
    }

    void ScriptWorld::Update(const float deltaTime)
    {
        if (!m_Initialized || !m_State || !m_Begun) return;

        SynchronizeInstances();
        ReloadChangedInstances();

        if (!std::isfinite(deltaTime) || deltaTime < 0.0f) return;

        for (RuntimeState::ScriptInstance& instance : m_State->Instances)
        {
            if (!instance.Started || instance.Faulted || !instance.OnUpdate) continue;

            const sol::protected_function_result result = (*instance.OnUpdate)(deltaTime);
            if (result.valid()) continue;

            const sol::error error = result;
            QUIET_CORE_ERROR("Script '{}' OnUpdate failed for actor '{}': {}. The instance will stop updating.", instance.ScriptAssetID, Core::UUIDToString(instance.ActorID), error.what());

            instance.Faulted = true;
        }
    }

    void ScriptWorld::DispatchPhysicsContactEvents()
    {
        if (!m_Initialized || !m_State || !m_Begun) return;

        const auto& contactEvents = m_Scene.GetPhysicsContactEvents();
        for (const Physics::PhysicsContactEvent& event : contactEvents)
        {
            const auto dispatchToActor = [&](const Core::UUID actorID, const Core::UUID otherActorID)
            {
                const auto iterator =
                    std::find_if(
                        m_State->Instances.begin(),
                        m_State->Instances.end(),
                        [actorID](const RuntimeState::ScriptInstance& instance)
                            {
                                return instance.ActorID == actorID;
                            });

                if (iterator == m_State->Instances.end()) return;
                RuntimeState::ScriptInstance& instance = *iterator;
                if (!instance.Started || instance.Faulted) return;

                sol::protected_function* callback = nullptr;
                const char* callbackName = nullptr;

                switch (event.Type)
                {
                    case Physics::PhysicsContactEventType::Started:
                        {
                            if (instance.OnCollisionEnter)
                            {
                                callback = &*instance.OnCollisionEnter;
                                callbackName = "OnCollisionEnter";
                            }
                            break;
                        }

                    case Physics::PhysicsContactEventType::Ended:
                        {
                            if (instance.OnCollisionExit)
                            {
                                callback = &*instance.OnCollisionExit;
                                callbackName = "OnCollisionExit";
                            }
                            break;
                        }
                }

                if (!callback) return;
                if (!m_Scene.FindActorByID(otherActorID)) return;
                sol::table otherActor = Bindings::CreateActorReference(m_State->Lua, m_Scene, otherActorID);
                const sol::protected_function_result result = (*callback)(otherActor);
                if (result.valid()) return;

                const sol::error error = result;
                QUIET_CORE_ERROR("Script '{}' {} failed for actor '{}' against actor '{}': {}. The instance will stop receiving runtime callbacks.",
                        instance.ScriptAssetID, callbackName, Core::UUIDToString(instance.ActorID), Core::UUIDToString(otherActorID), error.what());
                instance.Faulted = true;
            };

            dispatchToActor(event.ActorA, event.ActorB);
            dispatchToActor(event.ActorB, event.ActorA);
        }
    }

    void ScriptWorld::NotifyActorWillBeDestroyed(Core::UUID actorID)
    {
        if (!m_State) return;

        for (std::size_t index = 0; index < m_State->Instances.size(); ++index)
        {
            if (m_State->Instances[index].ActorID != actorID) continue;
            DestroyInstance(index);
            return;
        }

        m_State->FailedInstances.erase(actorID);
    }

    void ScriptWorld::End()
    {
        if (!m_State || !m_Begun) return;

        // Reverse creation order.
        while (!m_State->Instances.empty())
        {
            DestroyInstance(m_State->Instances.size() - 1);
        }

        m_State->FailedInstances.clear();
        m_Begun = false;
    }

    void ScriptWorld::Shutdown()
    {
        if (!m_State)
        {
            m_Initialized = false;
            m_Begun = false;
            return;
        }

        End();

        // Lua references must be released before the state itself.
        m_State->Instances.clear();
        m_State->FailedInstances.clear();
        m_State.reset();

        m_Initialized = false;
        m_Begun = false;

        QUIET_CORE_INFO("Shut down scripting world.");
    }

    bool ScriptWorld::CreateInstance(const Scene::Actor& actor, bool recordFailure)
    {
        if (!m_State) return false;

        const auto* script = actor.TryGetComponent<Scene::ScriptComponent>();
        if (!script || script->ScriptAssetID.empty()) return false;

        const Core::UUID actorID = actor.GetID();
        const std::string scriptAssetID = script->ScriptAssetID;
        const auto recordFailedInstance = [&]()
        {
            if (recordFailure)
            {
                m_State->FailedInstances[actorID] = scriptAssetID;
            }
        };

        const Project::AssetRecord* record = Project::AssetRegistryIO::FindByID(m_ProjectContext.GetAssetRegistry(), scriptAssetID);
        if (!record)
        {
            QUIET_CORE_WARN("Cannot create runtime script instance for actor '{}': script asset '{}' is not registered.", Core::UUIDToString(actorID), scriptAssetID);
            recordFailedInstance();
            return false;
        }

        if (record->Type != Project::AssetType::Script)
        {
            QUIET_CORE_WARN("Cannot create runtime script instance for actor '{}': asset '{}' is not a Script asset.", Core::UUIDToString(actorID), scriptAssetID);
            recordFailedInstance();
            return false;
        }

        if (record->Path.empty())
        {
            QUIET_CORE_WARN("Cannot create runtime script instance for actor '{}': script asset '{}' has no source path.", Core::UUIDToString(actorID), scriptAssetID);
            recordFailedInstance();
            return false;
        }

        std::filesystem::path sourcePath = record->Path;
        if (sourcePath.is_relative())
        {
            sourcePath = m_ProjectContext.GetRootPath() / sourcePath;
        }

        sourcePath = sourcePath.lexically_normal();
        std::error_code filesystemError;
        if (!std::filesystem::is_regular_file(sourcePath, filesystemError) || filesystemError)
        {
            QUIET_CORE_WARN("Cannot create runtime script instance for actor '{}': script source '{}' is unavailable.", Core::UUIDToString(actorID), sourcePath.string());
            recordFailedInstance();
            return false;
        }

        filesystemError.clear();
        const std::filesystem::file_time_type sourceWriteTime = std::filesystem::last_write_time(sourcePath, filesystemError);
        const std::filesystem::file_time_type effectiveWriteTime = filesystemError ? std::filesystem::file_time_type::min() : sourceWriteTime;

        sol::environment environment(m_State->Lua, sol::create, m_State->Lua.globals());
        Bindings::InstallPropertyBindings(m_State->Lua, environment, m_Scene, m_ProjectContext, *script);
        Bindings::InstallActorBindings(m_State->Lua, environment, m_Scene, actorID);
        Bindings::InstallSceneBindings(m_State->Lua, environment, m_Scene);
        Bindings::InstallInputBindings(m_State->Lua, environment, m_Input);
        const sol::protected_function_result loadResult = m_State->Lua.safe_script_file(sourcePath.string(), environment, sol::script_pass_on_error, sol::load_mode::text);

        if (!loadResult.valid())
        {
            const sol::error error = loadResult;
            QUIET_CORE_ERROR("Failed to load script '{}' for actor '{}': {}", scriptAssetID, Core::UUIDToString(actorID), error.what());
            recordFailedInstance();
            return false;
        }

        RuntimeState::ScriptInstance instance(actorID, scriptAssetID, sourcePath, effectiveWriteTime, std::move(environment));

        const auto captureCallback = [&](const char* name, std::optional<sol::protected_function>& destination)
        {
            const sol::object value = instance.Environment[name];
            if (!value.valid() || value == sol::nil) return;

            if (value.get_type() != sol::type::function)
            {
                QUIET_CORE_WARN("Script '{}' on actor '{}' defines '{}' but it is not a function.", instance.ScriptAssetID, Core::UUIDToString(instance.ActorID), name);
                return;
            }

            destination = value.as<sol::protected_function>();
        };

        captureCallback("OnCreate", instance.OnCreate);
        captureCallback("OnUpdate", instance.OnUpdate);
        captureCallback("OnDestroy", instance.OnDestroy);

        captureCallback("OnCollisionEnter", instance.OnCollisionEnter);
        captureCallback("OnCollisionExit", instance.OnCollisionExit);

        m_State->Instances.push_back(std::move(instance));
        m_State->FailedInstances.erase(actorID);

        return true;
    }

    void ScriptWorld::StartInstance(std::size_t index)
    {
        if (!m_State || index >= m_State->Instances.size()) return;

        RuntimeState::ScriptInstance& instance = m_State->Instances[index];
        if (instance.Started || instance.Faulted) return;
        if (instance.OnCreate)
        {
            const sol::protected_function_result result = (*instance.OnCreate)();
            if (!result.valid())
            {
                const sol::error error = result;
                QUIET_CORE_ERROR("Script '{}' OnCreate failed for actor '{}': {}", instance.ScriptAssetID, Core::UUIDToString(instance.ActorID), error.what());
                instance.Faulted = true;
                return;
            }
        }

        instance.Started = true;
    }

    void ScriptWorld::DestroyInstance(std::size_t index)
    {
        if (!m_State || index >= m_State->Instances.size()) return;

        RuntimeState::ScriptInstance& instance = m_State->Instances[index];
        if (instance.Started && instance.OnDestroy)
        {
            const sol::protected_function_result result = (*instance.OnDestroy)();
            if (!result.valid())
            {
                const sol::error error = result;
                QUIET_CORE_ERROR("Script '{}' OnDestroy failed for actor '{}': {}", instance.ScriptAssetID, Core::UUIDToString(instance.ActorID), error.what());
            }
        }

        instance.Started = false;

        const Core::UUID actorID = instance.ActorID;

        m_State->Instances.erase(m_State->Instances.begin() + static_cast<std::ptrdiff_t>(index));
        m_State->FailedInstances.erase(actorID);
    }

    void ScriptWorld::SynchronizeInstances()
    {
        if (!m_State || !m_Begun) return;

        std::size_t createdCount = 0;
        std::size_t removedCount = 0;

        // Remove runtime instances which no longer match
        // the current runtime Scene state.
        for (std::size_t index = 0; index < m_State->Instances.size();)
        {
            const RuntimeState::ScriptInstance& instance = m_State->Instances[index];
            const Scene::Actor* actor = m_Scene.FindActorByID(instance.ActorID);
            const Scene::ScriptComponent* script = actor ? actor->TryGetComponent<Scene::ScriptComponent>() : nullptr;

            const bool shouldRemove = !actor || !script || script->ScriptAssetID.empty() || script->ScriptAssetID != instance.ScriptAssetID;
            if (!shouldRemove)
            {
                ++index;
                continue;
            }

            DestroyInstance(index);
            ++removedCount;
        }

        // Failed loads should only remain suppressed while
        // the Actor still references exactly the same script.
        for (auto iterator = m_State->FailedInstances.begin(); iterator != m_State->FailedInstances.end();)
        {
            const Scene::Actor* actor = m_Scene.FindActorByID(iterator->first);
            const Scene::ScriptComponent* script = actor ? actor->TryGetComponent<Scene::ScriptComponent>() : nullptr;

            if (!script || script->ScriptAssetID.empty() || script->ScriptAssetID != iterator->second)
            {
                iterator = m_State->FailedInstances.erase(iterator);
                continue;
            }

            ++iterator;
        }

        // Create instances for new or newly-scripted Actors.
        for (const Scene::Actor& actor : m_Scene.GetActors())
        {
            const Scene::ScriptComponent* script = actor.TryGetComponent<Scene::ScriptComponent>();
            if (!script || script->ScriptAssetID.empty()) continue;

            const bool alreadyActive =
                std::any_of(
                    m_State->Instances.begin(),
                    m_State->Instances.end(),
                    [&actor](const RuntimeState::ScriptInstance& instance)
                    {
                        return instance.ActorID == actor.GetID();
                    });

            if (alreadyActive) continue;

            const auto failedIterator = m_State->FailedInstances.find(actor.GetID());
            if (failedIterator != m_State->FailedInstances.end() && failedIterator->second == script->ScriptAssetID)
            {
                continue;
            }

            if (!CreateInstance(actor)) continue;

            const std::size_t newIndex = m_State->Instances.size() - 1;

            StartInstance(newIndex);
            ++createdCount;
        }

        if (createdCount > 0 || removedCount > 0)
        {
            QUIET_CORE_INFO("Synchronized runtime script instances. created={}, removed={}, active={}", createdCount, removedCount, m_State->Instances.size());
        }
    }

    void ScriptWorld::ReloadChangedInstances()
    {
        if (!m_State || !m_Begun) return;
        std::size_t index = 0;

        while (index < m_State->Instances.size())
        {
            RuntimeState::ScriptInstance& current = m_State->Instances[index];
            std::error_code filesystemError;

            const auto writeTime = std::filesystem::last_write_time(current.SourcePath, filesystemError);
            if (filesystemError || writeTime == current.SourceWriteTime)
            {
                ++index;
                continue;
            }

            // Record the version we just attempted.
            //
            // If the new source contains an error, we keep the
            // old working instance and don't retry every frame.
            // Saving the file again changes its timestamp and
            // triggers another attempt.
            current.SourceWriteTime = writeTime;

            Scene::Actor* actor = m_Scene.FindActorByID(current.ActorID);
            if (!actor)
            {
                ++index;
                continue;
            }

            const Core::UUID actorID = current.ActorID;
            const std::string scriptAssetID = current.ScriptAssetID;
            const std::size_t previousSize = m_State->Instances.size();

            // Build a replacement without disturbing the currently running instance.
            // Reload failures must leave the old environment alive.
            if (!CreateInstance(*actor, false))
            {
                QUIET_CORE_WARN(
                    "Hot reload failed for script '{}' on actor '{}'. Keeping the previous runtime instance.",
                    scriptAssetID, Core::UUIDToString(actorID));
                ++index;
                continue;
            }

            if (m_State->Instances.size() !=
                previousSize + 1)
            {
                QUIET_CORE_ERROR(
                    "Hot reload for script '{}' on actor '{}' did not create the expected replacement instance.",
                    scriptAssetID, Core::UUIDToString(actorID));
                ++index;
                continue;
            }

            RuntimeState::ScriptInstance replacement = std::move(m_State->Instances.back());
            m_State->Instances.pop_back();

            // The replacement source has successfully parsed and
            // executed at file scope. The old lifecycle can now end.
            DestroyInstance(index);

            m_State->Instances.insert(m_State->Instances.begin() + static_cast<std::ptrdiff_t>(index), std::move(replacement));
            StartInstance(index);
            QUIET_CORE_INFO("Hot reloaded script '{}' for actor '{}'.", scriptAssetID, Core::UUIDToString(actorID));

            ++index;
        }
    }
}

#include "Scripting/ScriptWorld.h"

#include <cmath>
#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <sol/sol.hpp>

#include "Quiet/Core/Log.h"
#include "Quiet/Core/UUID.h"
#include "Quiet/Project/AssetRegistry.h"
#include "Quiet/Project/AssetType.h"
#include "Quiet/Project/ProjectContext.h"
#include "Scene/Components/ScriptComponent.h"
#include "Scripting/Bindings/ActorBindings.h"
#include "Scripting/Bindings/InputBindings.h"
#include "Scripting/Bindings/PhysicsBindings.h"
#include "Scene/Scene.h"

namespace Quiet::Scripting
{
    struct ScriptWorld::RuntimeState
    {
        struct ScriptInstance
        {
            ScriptInstance(const Core::UUID actorID, std::string scriptAssetID, sol::environment environment)
                : ActorID(actorID) , ScriptAssetID(std::move(scriptAssetID)) , Environment(std::move(environment))
            {}

            Core::UUID ActorID{};
            std::string ScriptAssetID;

            sol::environment Environment;

            std::optional<sol::protected_function> OnCreate;
            std::optional<sol::protected_function> OnUpdate;
            std::optional<sol::protected_function> OnDestroy;

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

    bool ScriptWorld::CreateInstance(const Scene::Actor& actor)
    {
        if (!m_State) return false;

        const auto* script = actor.TryGetComponent<Scene::ScriptComponent>();
        if (!script || script->ScriptAssetID.empty()) return false;

        const Core::UUID actorID = actor.GetID();
        const std::string scriptAssetID = script->ScriptAssetID;

        const Project::AssetRecord* record = Project::AssetRegistryIO::FindByID(m_ProjectContext.GetAssetRegistry(), scriptAssetID);
        if (!record)
        {
            QUIET_CORE_WARN("Cannot create runtime script instance for actor '{}': script asset '{}' is not registered.", Core::UUIDToString(actorID), scriptAssetID);
            m_State->FailedInstances[actorID] = scriptAssetID;
            return false;
        }

        if (record->Type != Project::AssetType::Script)
        {
            QUIET_CORE_WARN("Cannot create runtime script instance for actor '{}': asset '{}' is not a Script asset.", Core::UUIDToString(actorID), scriptAssetID);
            m_State->FailedInstances[actorID] = scriptAssetID;
            return false;
        }

        if (record->Path.empty())
        {
            QUIET_CORE_WARN("Cannot create runtime script instance for actor '{}': script asset '{}' has no source path.", Core::UUIDToString(actorID), scriptAssetID);
            m_State->FailedInstances[actorID] = scriptAssetID;
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
            m_State->FailedInstances[actorID] = scriptAssetID;
            return false;
        }

        sol::environment environment(m_State->Lua, sol::create, m_State->Lua.globals());
        Bindings::InstallActorBindings(m_State->Lua, environment, m_Scene, actorID);
        Bindings::InstallInputBindings(m_State->Lua, environment, m_Input);
        Bindings::InstallPhysicsBindings(m_State->Lua, environment, m_Scene, actorID);
        const sol::protected_function_result loadResult = m_State->Lua.safe_script_file(sourcePath.string(), environment, sol::script_pass_on_error, sol::load_mode::text);

        if (!loadResult.valid())
        {
            const sol::error error = loadResult;
            QUIET_CORE_ERROR("Failed to load script '{}' for actor '{}': {}", scriptAssetID, Core::UUIDToString(actorID), error.what());
            m_State->FailedInstances[actorID] = scriptAssetID;
            return false;
        }

        RuntimeState::ScriptInstance instance(actorID, scriptAssetID, std::move(environment));

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
}

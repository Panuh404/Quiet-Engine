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
    };

    ScriptWorld::ScriptWorld(Scene::Scene& scene, const Project::ProjectContext& projectContext)
        : m_Scene(scene), m_ProjectContext(projectContext)
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
        m_State->Instances.clear();

        std::size_t loadedCount = 0;
        std::size_t skippedCount = 0;

        for (const Scene::Actor& actor : m_Scene.GetActors())
        {
            const auto* script = actor.TryGetComponent<Scene::ScriptComponent>();
            if (!script || script->ScriptAssetID.empty()) continue;

            const Project::AssetRecord* record = Project::AssetRegistryIO::FindByID(m_ProjectContext.GetAssetRegistry(), script->ScriptAssetID);
            if (!record)
            {
                QUIET_CORE_WARN("Cannot create runtime script instance for actor '{}': script asset '{}' is not registered.",
                    Core::UUIDToString(actor.GetID()), script->ScriptAssetID);

                ++skippedCount;
                continue;
            }

            if (record->Type != Project::AssetType::Script)
            {
                QUIET_CORE_WARN("Cannot create runtime script instance for actor '{}': asset '{}' is not a Script asset.",
                    Core::UUIDToString(actor.GetID()), script->ScriptAssetID);

                ++skippedCount;
                continue;
            }

            if (record->Path.empty())
            {
                QUIET_CORE_WARN("Cannot create runtime script instance for actor '{}': script asset '{}' has no source path.",
                    Core::UUIDToString(actor.GetID()), script->ScriptAssetID);

                ++skippedCount;
                continue;
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
                QUIET_CORE_WARN("Cannot create runtime script instance for actor '{}': script source '{}' is unavailable.",
                    Core::UUIDToString(actor.GetID()), sourcePath.string());

                ++skippedCount;
                continue;
            }

            sol::environment environment(m_State->Lua, sol::create, m_State->Lua.globals());

            // Bindings
            Bindings::InstallActorBindings(m_State->Lua, environment, m_Scene, actor.GetID());

            const sol::protected_function_result loadResult = m_State->Lua.safe_script_file(sourcePath.string(), environment, sol::script_pass_on_error, sol::load_mode::text);
            if (!loadResult.valid())
            {
                const sol::error error = loadResult;
                QUIET_CORE_ERROR("Failed to load script '{}' for actor '{}': {}", script->ScriptAssetID, Core::UUIDToString(actor.GetID()), error.what());

                ++skippedCount;
                continue;
            }

            RuntimeState::ScriptInstance instance(actor.GetID(), script->ScriptAssetID, std::move(environment));
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
            ++loadedCount;
        }

        QUIET_CORE_INFO("Built runtime script instances. loaded={}, skipped={}", loadedCount, skippedCount);
    }

    void ScriptWorld::Begin()
    {
        if (!m_Initialized || !m_State || m_Begun) return;
        m_Begun = true;

        for (RuntimeState::ScriptInstance& instance : m_State->Instances)
        {
            if (instance.OnCreate)
            {
                const sol::protected_function_result result = (*instance.OnCreate)();
                if (!result.valid())
                {
                    const sol::error error = result;
                    QUIET_CORE_ERROR("Script '{}' OnCreate failed for actor '{}': {}", instance.ScriptAssetID, Core::UUIDToString(instance.ActorID), error.what());
                    instance.Faulted = true;
                    continue;
                }
            }

            instance.Started = true;
        }
    }

    void ScriptWorld::Update(const float deltaTime)
    {
        if (!m_Initialized || !m_State || !m_Begun || !std::isfinite(deltaTime) || deltaTime < 0.0f) return;

        for (RuntimeState::ScriptInstance& instance : m_State->Instances)
        {
            if (!instance.Started || instance.Faulted || !instance.OnUpdate) continue;

            const sol::protected_function_result result = (*instance.OnUpdate)(deltaTime);
            if (result.valid()) continue;

            const sol::error error = result;

            QUIET_CORE_ERROR("Script '{}' OnUpdate failed for actor '{}': {}. The instance will stop updating.",
                instance.ScriptAssetID, Core::UUIDToString(instance.ActorID), error.what());

            // Prevent a broken OnUpdate from flooding the log every frame.
            // OnDestroy can still run during runtime shutdown.
            instance.Faulted = true;
        }
    }

    void ScriptWorld::End()
    {
        if (!m_State || !m_Begun) return;

        // Tear down in reverse creation order.
        for (auto iterator = m_State->Instances.rbegin(); iterator != m_State->Instances.rend(); ++iterator)
        {
            RuntimeState::ScriptInstance& instance = *iterator;
            if (!instance.Started || !instance.OnDestroy) continue;

            const sol::protected_function_result result = (*instance.OnDestroy)();
            if (!result.valid())
            {
                const sol::error error = result;
                QUIET_CORE_ERROR("Script '{}' OnDestroy failed for actor '{}': {}", instance.ScriptAssetID, Core::UUIDToString(instance.ActorID), error.what());
            }

            instance.Started = false;
        }

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
        m_State.reset();

        m_Initialized = false;
        m_Begun = false;

        QUIET_CORE_INFO("Shut down scripting world.");
    }
}

#include "Scripting/ScriptWorld.h"

#include <stdexcept>
#include <string>

#include <sol/sol.hpp>
#include "Quiet/Core/Log.h"

namespace Quiet::Scripting
{
    struct ScriptWorld::RuntimeState
    {
        sol::state Lua;
    };

    ScriptWorld::ScriptWorld()
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

    void ScriptWorld::Shutdown()
    {
        if (!m_State)
        {
            m_Initialized = false;
            return;
        }

        m_State.reset();
        m_Initialized = false;

        QUIET_CORE_INFO("Shut down scripting world.");
    }
}

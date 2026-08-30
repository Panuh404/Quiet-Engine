#pragma once

#include <memory>

namespace Quiet::Scripting
{
    class ScriptWorld
    {
    public:
        ScriptWorld();
        ~ScriptWorld();

        ScriptWorld(const ScriptWorld&) = delete;
        ScriptWorld& operator=(const ScriptWorld&) = delete;
        ScriptWorld(ScriptWorld&&) = delete;
        ScriptWorld& operator=(ScriptWorld&&) = delete;

        [[nodiscard]] bool IsInitialized() const { return m_Initialized; }

    private:
        struct RuntimeState;

        void Initialize();
        void Shutdown();

    private:
        std::unique_ptr<RuntimeState> m_State;
        bool m_Initialized = false;
    };
}
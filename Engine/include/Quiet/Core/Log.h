#pragma once

#include <memory>

#include <spdlog/logger.h>

namespace Quiet::Core
{
    class Log
    {
    public:
        static void Init();
        static void Shutdown();

        static std::shared_ptr<spdlog::logger>& GetCoreLogger();
        static std::shared_ptr<spdlog::logger>& GetClientLogger();

    private:
        static std::shared_ptr<spdlog::logger> s_CoreLogger;
        static std::shared_ptr<spdlog::logger> s_ClientLogger;
    };
}


// Core
#define QUIET_CORE_TRACE(...)    ::Quiet::Core::Log::GetCoreLogger()->trace(__VA_ARGS__)
#define QUIET_CORE_INFO(...)     ::Quiet::Core::Log::GetCoreLogger()->info(__VA_ARGS__)
#define QUIET_CORE_WARN(...)     ::Quiet::Core::Log::GetCoreLogger()->warn(__VA_ARGS__)
#define QUIET_CORE_ERROR(...)    ::Quiet::Core::Log::GetCoreLogger()->error(__VA_ARGS__)
#define QUIET_CORE_CRITICAL(...) ::Quiet::Core::Log::GetCoreLogger()->critical(__VA_ARGS__)

// Client
#define QUIET_TRACE(...)         ::Quiet::Core::Log::GetClientLogger()->trace(__VA_ARGS__)
#define QUIET_INFO(...)          ::Quiet::Core::Log::GetClientLogger()->info(__VA_ARGS__)
#define QUIET_WARN(...)          ::Quiet::Core::Log::GetClientLogger()->warn(__VA_ARGS__)
#define QUIET_ERROR(...)         ::Quiet::Core::Log::GetClientLogger()->error(__VA_ARGS__)
#define QUIET_CRITICAL(...)      ::Quiet::Core::Log::GetClientLogger()->critical(__VA_ARGS__)
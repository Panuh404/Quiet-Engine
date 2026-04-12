#pragma once

#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include <spdlog/logger.h>
#include <spdlog/common.h>
#include <spdlog/sinks/base_sink.h>

namespace Quiet::Core
{
    struct ConsoleLogMessage
    {
        spdlog::level::level_enum Level = spdlog::level::info;
        std::string Text;
    };

    class ImGuiConsoleSink final : public spdlog::sinks::base_sink<std::mutex>
    {
    public:
        explicit ImGuiConsoleSink(std::size_t maxEntries = 2000);

        std::vector<ConsoleLogMessage> GetSnapshot();
        void Clear();

    protected:
        void sink_it_(const spdlog::details::log_msg& msg) override;
        void flush_() override;

    private:
        std::size_t m_MaxEntries = 2000;
        std::deque<ConsoleLogMessage> m_Entries;
    };

    class Log
    {
    public:
        static void Init();
        static void Shutdown();

        static std::shared_ptr<spdlog::logger>& GetCoreLogger();
        static std::shared_ptr<spdlog::logger>& GetClientLogger();
        static std::shared_ptr<ImGuiConsoleSink>& GetConsoleSink();

    private:
        static std::shared_ptr<spdlog::logger> s_CoreLogger;
        static std::shared_ptr<spdlog::logger> s_ClientLogger;
        static std::shared_ptr<ImGuiConsoleSink> s_ConsoleSink;
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
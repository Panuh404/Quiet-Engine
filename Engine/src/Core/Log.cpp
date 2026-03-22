#include "Quiet/Core/Log.h"

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <vector>

namespace Quiet::Core
{
    std::shared_ptr<spdlog::logger> Log::s_CoreLogger;
    std::shared_ptr<spdlog::logger> Log::s_ClientLogger;

    void Log::Init()
    {
        if (s_CoreLogger || s_ClientLogger) return;

        auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        consoleSink->set_pattern("[%T] [%n] [%^%l%$] %v");

        std::vector<spdlog::sink_ptr> sinks = { consoleSink };
        s_CoreLogger = std::make_shared<spdlog::logger>("QUIET", sinks.begin(), sinks.end());
        s_ClientLogger = std::make_shared<spdlog::logger>("APP", sinks.begin(), sinks.end());

#ifndef NDEBUG
        s_CoreLogger->set_level(spdlog::level::trace);
        s_ClientLogger->set_level(spdlog::level::trace);
#else
        s_CoreLogger->set_level(spdlog::level::info);
        s_ClientLogger->set_level(spdlog::level::info);
#endif

        s_CoreLogger->flush_on(spdlog::level::warn);
        s_ClientLogger->flush_on(spdlog::level::warn);

        spdlog::register_logger(s_CoreLogger);
        spdlog::register_logger(s_ClientLogger);
    }

    void Log::Shutdown()
    {
        s_ClientLogger.reset();
        s_CoreLogger.reset();
        spdlog::shutdown();
    }

    std::shared_ptr<spdlog::logger>& Log::GetCoreLogger()
    {
        return s_CoreLogger;
    }

    std::shared_ptr<spdlog::logger>& Log::GetClientLogger()
    {
        return s_ClientLogger;
    }
}
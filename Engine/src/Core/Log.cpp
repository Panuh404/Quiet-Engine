#include "Quiet/Core/Log.h"

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <vector>

namespace Quiet::Core
{
    std::shared_ptr<spdlog::logger> Log::s_CoreLogger;
    std::shared_ptr<spdlog::logger> Log::s_ClientLogger;
    std::shared_ptr<ImGuiConsoleSink> Log::s_ConsoleSink;

    ImGuiConsoleSink::ImGuiConsoleSink(std::size_t maxEntries) : m_MaxEntries(maxEntries) {}

    std::vector<ConsoleLogMessage> ImGuiConsoleSink::GetSnapshot()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return std::vector<ConsoleLogMessage>(m_Entries.begin(), m_Entries.end());
    }

    void ImGuiConsoleSink::Clear()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        m_Entries.clear();
    }

    void ImGuiConsoleSink::sink_it_(const spdlog::details::log_msg& msg)
    {
        spdlog::memory_buf_t formatted;
        formatter_->format(msg, formatted);

        ConsoleLogMessage entry{};
        entry.Level = msg.level;
        entry.Text = std::string(formatted.data(), formatted.size());

        m_Entries.push_back(std::move(entry));

        while (m_Entries.size() > m_MaxEntries)
        {
            m_Entries.pop_front();
        }
    }

    void ImGuiConsoleSink::flush_() {}

    void Log::Init()
    {
        if (s_CoreLogger || s_ClientLogger) return;

        auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        consoleSink->set_pattern("[%T] [%n] [%^%l%$] %v");

        s_ConsoleSink = std::make_shared<ImGuiConsoleSink>(4000);
        s_ConsoleSink->set_pattern("[%T] [%n] [%l] %v");

        std::vector<spdlog::sink_ptr> sinks = { consoleSink, s_ConsoleSink };
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
        s_ConsoleSink.reset();
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

    std::shared_ptr<ImGuiConsoleSink> & Log::GetConsoleSink()
    {
        return s_ConsoleSink;
    }
}

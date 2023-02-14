#pragma once

#include "Quiet/Core/Core.h"
#include "spdlog/spdlog.h"

namespace Quiet
{
	class Log
	{
	public:
		static void Init();

		static std::shared_ptr<spdlog::logger>& GetCoreLogger() { return s_CoreLogger; }
		static std::shared_ptr<spdlog::logger>& GetClientLogger() { return s_ClientLogger; }

	private:
		static std::shared_ptr<spdlog::logger> s_CoreLogger;
		static std::shared_ptr<spdlog::logger> s_ClientLogger;
	};
}

//-----------------------------------------------------------------------------
// [MACROS] Core
//-----------------------------------------------------------------------------
#define QT_CORE_INFO(...)		::Quiet::Log::GetCoreLogger()->info(__VA_ARGS__)
#define QT_CORE_WARN(...)		::Quiet::Log::GetCoreLogger()->warn(__VA_ARGS__)
#define QT_CORE_TRACE(...)		::Quiet::Log::GetCoreLogger()->trace(__VA_ARGS__)
#define QT_CORE_ERROR(...)		::Quiet::Log::GetCoreLogger()->error(__VA_ARGS__)
#define QT_CORE_CRITICAL(...)	::Quiet::Log::GetCoreLogger()->critical(__VA_ARGS__)

//-----------------------------------------------------------------------------
// [MACROS] Client
//-----------------------------------------------------------------------------			   
#define QT_INFO(...)			::Quiet::Log::GetClientLogger()->info(__VA_ARGS__)
#define QT_WARN(...)			::Quiet::Log::GetClientLogger()->warn(__VA_ARGS__)
#define QT_TRACE(...)			::Quiet::Log::GetClientLogger()->trace(__VA_ARGS__)
#define QT_ERROR(...)			::Quiet::Log::GetClientLogger()->error(__VA_ARGS__)
#define QT_CRITICAL(...)		::Quiet::Log::GetClientLogger()->critical(__VA_ARGS__)
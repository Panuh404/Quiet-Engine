#pragma once

#include <memory>

// WINDOWS PLATFORM
#ifdef QT_PLATFORM_WINDOWS
	// FOR SHARED LIB ONLY (.dll)
	#ifdef QT_BUILD_DLL
		#define QUIET_API __declspec(dllexport)	// Export to DLL
	#else
		#define QUIET_API __declspec(dllimport)	// Import from DLL
	#endif

#else
	#error QUIET ONLY SUPPORTS WINDOWS!
#endif

// Binds
#define BIT(x) (1 << x)
#define BIND_EVENT_FN(fn) std::bind(&fn, this, std::placeholders::_1)

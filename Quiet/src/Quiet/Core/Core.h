#pragma once

#include <memory>

//-----------------------------------------------------------------------------
// [PLATFORM] Windows
//-----------------------------------------------------------------------------
#ifdef QT_PLATFORM_WINDOWS
	#if QT_DYNAMIC_LINK
		// FOR SHARED LIB ONLY (.dll)
		#ifdef QT_BUILD_DLL
			#define QUIET_API __declspec(dllexport)	// Export to DLL
		#else
			#define QUIET_API __declspec(dllimport)	// Import from DLL
		#endif
	#else
		#define QUIET_API
	#endif
#else
	#error QUIET ONLY SUPPORTS WINDOWS!
#endif

//-----------------------------------------------------------------------------
// [DEBUG]
//----------------------------------------------------------------------------- 
#ifdef QT_DEBUG
	#define QT_ENABLE_ASSERTS
#endif

//-----------------------------------------------------------------------------
// [ASSERTIONS] For Visual Studio Only
//----------------------------------------------------------------------------- 
#ifdef QT_ENABLE_ASSERTS
#define QT_ASSERT(x, ...)		{ if(!(x)) { QT_ERROR("Assertion Failed: {0}", __VA_ARGS__); __debugbreak(); } }
#define QT_CORE_ASSERT(x, ...)	{ if(!(x)) { QT_CORE_ERROR("Assertion Failed: {0}", __VA_ARGS__); __debugbreak(); } }
#else
#define QT_ASSERT(x, ...)
#define QT_CORE_ASSERT(x, ...)
#endif

//-----------------------------------------------------------------------------
// [BINDS]
//----------------------------------------------------------------------------- 
#define BIT(x) (1 << x)
#define BIND_EVENT_FN(fn) std::bind(&fn, this, std::placeholders::_1)

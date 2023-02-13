#pragma once

#include <memory>

//-----------------------------------------------------------------------------
// [PLATFORM] Windows
//-----------------------------------------------------------------------------
// WINDOWS
#ifdef _WIN32
	// Windows x64/x86
	#ifdef _WIN64
		// Windows x64
		#define HZ_PLATFORM_WINDOWS
	#else
		// Windows x86
		#error "x86 Builds are not supported!"
	#endif
// APPLE
#elif defined(__APPLE__) || defined(__MACH__)
	#include <TargetConditionals.h>
	// TARGET_OS_MAC exists on all the platforms
	// so we must check all of them (in this order)
	// to ensure that we're running on MAC
	// and not some other Apple platform
	#if TARGET_IPHONE_SIMULATOR == 1
		#error "IOS simulator is not supported!"
	#elif TARGET_OS_IPHONE == 1
		#define HZ_PLATFORM_IOS
		#error "IOS is not supported!"
	#elif TARGET_OS_MAC == 1
		#define HZ_PLATFORM_MACOS
		#error "MacOS is not supported!"
	#else
		#error "Unknown Apple platform!"
	#endif

// ANDROID
#elif defined(__ANDROID__)
	// We also have to check __ANDROID__ before __linux__
	// since android is based on the linux kernel
	// it has __linux__ defined
	#define HZ_PLATFORM_ANDROID
	#error "Android is not supported!"

// LINUX
#elif defined(__linux__)
	#define HZ_PLATFORM_LINUX
	#error "Linux is not supported!"

// OTHER
#else
	// Unknown compiler/platform
	#error "Unknown platform!"
#endif


//-----------------------------------------------------------------------------
// [DLL] 
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

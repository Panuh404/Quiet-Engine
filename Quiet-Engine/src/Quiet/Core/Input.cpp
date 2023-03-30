#include "qtpch.h"

#include "Quiet/Core/Input.h"

#ifdef QT_PLATFORM_WINDOWS
#include "Platform/Windows/WindowsInput.h"
#endif

namespace Quiet
{
	std::unique_ptr<Input> Input::s_Instance = Input::Create();

	std::unique_ptr<Input> Input::Create()
	{
		#ifdef QT_PLATFORM_WINDOWS
			return std::make_unique<WindowsInput>();
		#else
			QT_CORE_ASSERT(false, "Unknown platform!");
			return nullptr;
		#endif
	}

}
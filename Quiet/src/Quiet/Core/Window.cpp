#include "qtpch.h"
#include "Quiet/Core/Window.h"

#ifdef QT_PLATFORM_WINDOWS
	#include "Platform/Windows/WindowsWindow.h"
#endif

namespace Quiet
{
	std::unique_ptr<Window> Window::Create(const WindowProps& props)
	{
		#ifdef QT_PLATFORM_WINDOWS
			return std::make_unique<WindowsWindow>(props);
		#else
			QT_CORE_ASSERT(false, "Unknown platform!");
			return nullptr;
		#endif
	}
}
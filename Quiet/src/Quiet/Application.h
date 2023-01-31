#pragma once

#include "Quiet/Core.h"
#include "Quiet/Window.h"
#include "Events/Event.h"

namespace Quiet
{
	class QUIET_API Application
	{
	public:
		Application();
		virtual ~Application();

		void Run();
	private:
		std::unique_ptr<Window> m_Window;
		bool m_Running = true;
	};

	// to be defined in client
	Application* CreateApplication();
}
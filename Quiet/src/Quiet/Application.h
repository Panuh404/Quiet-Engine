#pragma once

#include "Quiet/Core.h"
#include "Quiet/Window.h"
#include "Events/Event.h"
#include "Quiet/Events/ApplicationEvent.h"

namespace Quiet
{
	class QUIET_API Application
	{
	public:
		Application();
		virtual ~Application();

		void Run();
		void OnEvent(Event& e);
	private:
		bool OnWindowClose(WindowCloseEvent& e);

		std::unique_ptr<Window> m_Window;
		bool m_Running = true;
	};

	// to be defined in client
	Application* CreateApplication();
}
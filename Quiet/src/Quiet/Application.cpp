#include "Application.h"

#include "Quiet/Events/ApplicationEvent.h"
#include "Quiet/Log.h"

namespace Quiet
{
	Application::Application()
	{

	}

	Application::~Application()
	{

	}

	void Application::Run()
	{
		WindowResizeEvent e(1280, 720);
		QT_TRACE(e);
		
		while (true);
	}
}

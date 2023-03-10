#pragma once
#include "Quiet/Core/Core.h"

#ifdef QT_PLATFORM_WINDOWS

extern Quiet::Application* Quiet::CreateApplication();

int main(int argc, char** argv)
{
	Quiet::Log::Init();

	QT_PROFILE_BEGIN_SESSION("Startup", "EngineProfile-Startup.json");
	auto application = Quiet::CreateApplication();
	QT_PROFILE_END_SESSION();

	QT_PROFILE_BEGIN_SESSION("Runtime", "EngineProfile-Runtime.json");
	application->Run();
	QT_PROFILE_END_SESSION();

	QT_PROFILE_BEGIN_SESSION("Shutdown", "EngineProfile-Shutdown.json");
	delete application;
	QT_PROFILE_END_SESSION();


}

#endif


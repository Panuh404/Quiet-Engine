#pragma once
#include "Quiet/Core/Core.h"
#include "Quiet/Core/Application.h"

#ifdef QT_PLATFORM_WINDOWS

extern Quiet::Application* Quiet::CreateApplication();

int main(int argc, char** argv)
{
	Quiet::Log::Init();

	QT_PROFILE_BEGIN_SESSION("Startup", "EngineProfile-Startup.json");
	auto APP = Quiet::CreateApplication();
	QT_PROFILE_END_SESSION();

	QT_PROFILE_BEGIN_SESSION("Runtime", "EngineProfile-Runtime.json");
	APP->Run();
	QT_PROFILE_END_SESSION();

	QT_PROFILE_BEGIN_SESSION("Shutdown", "EngineProfile-Shutdown.json");
	delete APP;
	QT_PROFILE_END_SESSION();
}

#endif


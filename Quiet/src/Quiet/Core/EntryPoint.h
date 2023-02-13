#pragma once

#include "Quiet/Core/Application.h"
#include "Quiet/Core/Log.h"

#ifdef QT_PLATFORM_WINDOWS

extern Quiet::Application* Quiet::CreateApplication();

int main(int argc, char** argv)
{
	Quiet::Log::Init();
	QT_CORE_WARN("Initialized Log");
	QT_INFO("QUIET_ENGINE");

	auto application = Quiet::CreateApplication();
	application->Run();
	delete application;
}

#else
	#error QUIET ONLY SUPPORTS WINDOWS!

#endif


#pragma once

#ifdef QT_PLATFORM_WINDOWS

extern Quiet::Application* Quiet::CreateApplication();

int main(int argc, char** argv)
{
	Quiet::Log::Init();
	QT_CORE_WARN("INITIALIZED LOG!");
	QT_INFO("WORKING");

	auto app = Quiet::CreateApplication();
	app->Run();
	delete app;
}

#else
	#error QUIET ONLY SUPPORTS WINDOWS!
#endif


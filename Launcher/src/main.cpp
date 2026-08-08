#include <exception>

#include "LauncherApplication.h"

#include "Quiet/Core/Log.h"

int main()
{
    Quiet::Core::Log::Init();
    int exitCode = 0;

    try
    {
        Quiet::Launcher::LauncherApplication application;
        exitCode = application.Run();
    }
    catch (const std::exception& exception)
    {
        QUIET_CORE_CRITICAL("Quiet Launcher failed: {}", exception.what());
        exitCode = 1;
    }
    catch (...)
    {
        QUIET_CORE_CRITICAL("Quiet Launcher failed with an unknown error.");
        exitCode = 1;
    }

    Quiet::Core::Log::Shutdown();
    return exitCode;
}
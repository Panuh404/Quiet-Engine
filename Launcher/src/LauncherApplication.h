#pragma once

#include <memory>

namespace Quiet::Platform
{
    class Window;
}

namespace Quiet::Launcher
{
    class LauncherApplication
    {
    public:
        LauncherApplication();
        ~LauncherApplication();

        LauncherApplication(const LauncherApplication&) = delete;
        LauncherApplication& operator=(const LauncherApplication&) = delete;
        LauncherApplication(LauncherApplication&&) = delete;
        LauncherApplication& operator=(LauncherApplication&&) = delete;

        int Run();

    private:
        std::unique_ptr<Platform::Window> m_Window;
    };
}
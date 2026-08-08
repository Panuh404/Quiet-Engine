#include "LauncherApplication.h"

#include <stdexcept>

#include "Quiet/Core/Log.h"
#include "Quiet/Platform/Window.h"

namespace Quiet::Launcher
{
    LauncherApplication::LauncherApplication()
    {
        Platform::WindowDesc windowDesc{};

        windowDesc.Width = 1100;
        windowDesc.Height = 700;

        windowDesc.Title = "Quiet Launcher";
        windowDesc.IconPath = "Resources/Textures/Editor/quiet-icon.png";
        windowDesc.Resizable = true;
        windowDesc.StartMaximized = false;

        m_Window = std::make_unique<Platform::Window>(windowDesc);
        if (!m_Window->IsVulkanSupported())
        {
            throw std::runtime_error("GLFW reports that Vulkan is not available. Check the Vulkan loader, drivers, or SDK.");
        }

        QUIET_CORE_INFO("Initialized Quiet Launcher window.");
    }

    LauncherApplication::~LauncherApplication() = default;

    int LauncherApplication::Run()
    {
        if (!m_Window) return 1;
        QUIET_CORE_INFO("Starting Quiet Launcher.");

        while (!m_Window->ShouldClose())
        {
            m_Window->WaitEvents();
        }

        QUIET_CORE_INFO("Closing Quiet Launcher.");
        return 0;
    }
}
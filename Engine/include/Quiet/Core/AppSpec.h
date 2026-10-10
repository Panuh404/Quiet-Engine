#pragma once

#include <filesystem>
#include <cstdint>
#include <string>

namespace Quiet::Core
{
    /**
     * @brief Selects Editor or Sandbox application behavior.
     * @ingroup quiet_core
     */
    enum class AppMode : std::uint8_t
    {
        Sandbox = 0,
        Editor
    };

    /**
     * @brief Supplies initial window configuration and mode-specific startup inputs.
     * @ingroup quiet_core
     *
     * ProjectFilePath selects an editable project for Editor startup.
     * RuntimePackagePath selects a runtime package directory for Sandbox startup.
     * Requested window dimensions do not guarantee the resulting maximized size.
     */
    struct AppSpec
    {
        std::string Name = "Quiet Application";
        std::string WindowIconPath;

        std::filesystem::path ProjectFilePath;
        std::filesystem::path RuntimePackagePath;

        std::uint32_t Width = 1280;
        std::uint32_t Height = 720;
        bool StartMaximized = true;
        AppMode Mode = AppMode::Sandbox;

        [[nodiscard]] bool IsSandbox() const { return Mode == AppMode::Sandbox; }
        [[nodiscard]] bool IsEditor() const { return Mode == AppMode::Editor; }
    };
}

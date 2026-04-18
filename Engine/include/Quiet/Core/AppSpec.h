#pragma once
#include <cstdint>
#include <string>

namespace Quiet::Core
{
    enum class AppMode : std::uint8_t
    {
        Sandbox = 0,
        Editor
    };

    struct AppSpec
    {
        std::string Name = "Quiet Application";
        std::uint32_t Width = 1280;
        std::uint32_t Height = 720;
        bool StartMaximized = true;
        AppMode Mode = AppMode::Sandbox;

        [[nodiscard]] bool IsSandbox() const { return Mode == AppMode::Sandbox; }
        [[nodiscard]] bool IsEditor() const { return Mode == AppMode::Editor; }
    };
}

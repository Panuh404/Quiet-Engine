#pragma once
#include <cstdint>
#include <string>

namespace Quiet::Core
{
    struct AppSpec
    {
        std::string Name = "Quiet Application";
        std::uint32_t Width = 1280;
        std::uint32_t Height = 720;
        bool StartMaximized = true;
    };
}

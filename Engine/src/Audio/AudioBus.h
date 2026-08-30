#pragma once

#include <cstdint>

namespace Quiet::Audio
{
    enum class AudioBus : std::uint8_t
    {
        Master,
        Music,
        SFX,
        UI
    };
}
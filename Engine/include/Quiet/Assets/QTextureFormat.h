#pragma once

#include <cstdint>

namespace Quiet::Assets
{
    inline constexpr std::uint32_t CurrentQTexturePayloadVersion = 1;

    enum class QTexturePayloadFormat : std::uint32_t
    {
        Unknown = 0,
        Ktx2 = 1
    };
}
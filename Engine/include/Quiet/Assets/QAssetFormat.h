#pragma once

#include <array>
#include <cstdint>

namespace Quiet::Assets
{
    inline constexpr std::array QAssetMagic = { 'Q', 'A', 'S', 'T' };
    inline constexpr std::uint16_t CurrentQAssetContainerVersion = 1;

    enum class QAssetPayloadType : std::uint16_t
    {
        Mesh = 1,
        Texture = 2
    };

    inline constexpr std::uint32_t QAssetFlagNone = 0;
    inline constexpr std::uint32_t QAssetFlagZstdCompressed = 1u << 0u;
    inline constexpr std::uint32_t QAssetKnownFlags = QAssetFlagZstdCompressed;
}
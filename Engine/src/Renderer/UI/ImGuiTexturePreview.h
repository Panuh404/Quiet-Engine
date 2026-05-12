#pragma once

#include <cstdint>
#include <string_view>

#include <imgui.h>

namespace Quiet::Renderer::Vulkan
{
    class TextureLibrary;
    struct TextureStats;
}

namespace Quiet::Renderer::UI
{
    [[nodiscard]] ImTextureID GetOrCreateTexturePreviewID(
        std::string_view textureAssetID,
        const Vulkan::TextureLibrary& textureLibrary
    );

    [[nodiscard]] ImVec2 CalculateTexturePreviewSize(
        std::uint32_t width,
        std::uint32_t height,
        float maxSide
    );

    void DrawTexturePreview(
        std::string_view textureAssetID,
        const Vulkan::TextureLibrary& textureLibrary,
        const Vulkan::TextureStats& textureStats,
        float uiScale,
        float maxPreviewSize = 240.0f
    );

    void ClearTexturePreviewCache(bool releaseDescriptors = true);
}
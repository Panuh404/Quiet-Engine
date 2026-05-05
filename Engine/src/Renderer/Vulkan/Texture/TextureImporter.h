#pragma once

#include <filesystem>

#include "Renderer/Vulkan/Texture/TextureImportResult.h"
#include "Renderer/Vulkan/Texture/TexturePixelData.h"

namespace Quiet::Renderer::Vulkan
{
    class TextureImporter
    {
    public:
        [[nodiscard]] static TextureImportResult LoadFromFile(const std::filesystem::path& path);
        [[nodiscard]] static TexturePixelData LoadPixelsFromFile(const std::filesystem::path& path);
    };
}
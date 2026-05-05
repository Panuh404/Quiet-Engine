#pragma once

#include <filesystem>

#include "Renderer/Vulkan/Texture/TextureImportResult.h"

namespace Quiet::Renderer::Vulkan
{
    class TextureImporter
    {
    public:
        [[nodiscard]] static TextureImportResult LoadFromFile(const std::filesystem::path& path);
    };
}
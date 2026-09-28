#pragma once

#include <filesystem>

#include "Quiet/Assets/TextureImportResult.h"
#include "Quiet/Assets/TexturePixelData.h"

namespace Quiet::Assets
{
    class TextureImporter
    {
    public:
        [[nodiscard]] static TextureImportResult LoadFromFile(const std::filesystem::path& path);

        [[nodiscard]] static TexturePixelData LoadPixelsFromFile(const std::filesystem::path& path);
    };
}
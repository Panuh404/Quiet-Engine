#pragma once

#include <filesystem>

#include "Quiet/Assets/FontAtlasData.h"

namespace Quiet::Assets
{
    /**
     * @brief Imports font coverage and metrics independently of rendering.
     * @ingroup quiet_assets
     */
    class FontImporter
    {
    public:
        /**
         * @brief Rasterizes printable ASCII from the first font face.
         * @param path Path to a trusted font file supported by stb_truetype.
         * @param pixelHeight Ascender-to-descender height in pixels.
         * @return Owned atlas data; inspect Succeeded() and Error.
         * @note Uses a fixed 512x512 atlas with one texel of packing padding.
         * @note Unsupported text codepoints resolve to '?' through GetGlyph().
         * @note Expected input and packing failures are returned as errors.
         *       C++ allocation exceptions propagate.
         * @note This is not a validation boundary for untrusted font files.
         */
        [[nodiscard]] static FontAtlasData LoadFromFile(const std::filesystem::path& path, float pixelHeight = 32.0f);
    };
}
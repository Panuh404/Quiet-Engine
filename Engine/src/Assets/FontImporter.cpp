#include "Quiet/Assets/FontImporter.h"

#include <array>
#include <cmath>
#include <fstream>
#include <vector>

#define STB_TRUETYPE_IMPLEMENTATION
#include <stb_truetype.h>

namespace Quiet::Assets
{
    FontAtlasData FontImporter::LoadFromFile(const std::filesystem::path& path, const float pixelHeight)
    {
        constexpr int atlasWidth = 512;
        constexpr int atlasHeight = 512;
        constexpr int padding = 1;

        FontAtlasData result{};

        if (!std::isfinite(pixelHeight) || pixelHeight <= 0.0f || pixelHeight > static_cast<float>(atlasHeight))
        {
            result.Error = "Font pixel height must be finite and in the range (0, 512].";
            return result;
        }

        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file)
        {
            result.Error = "Failed to open font file.";
            return result;
        }

        const std::streamoff fileSize = file.tellg();
        if (fileSize < 12)
        {
            result.Error = "Font file is empty or its header is incomplete.";
            return result;
        }

        std::vector<unsigned char> fontBytes(static_cast<std::size_t>(fileSize));

        file.seekg(0, std::ios::beg);
        if (!file.read(reinterpret_cast<char*>(fontBytes.data()), static_cast<std::streamsize>(fileSize)))
        {
            result.Error = "Failed to read font file.";
            return result;
        }

        const int fontOffset = stbtt_GetFontOffsetForIndex(fontBytes.data(), 0);

        stbtt_fontinfo font{};
        if (fontOffset < 0 || static_cast<std::size_t>(fontOffset) >= fontBytes.size() || !stbtt_InitFont(&font, fontBytes.data(), fontOffset))
        {
            result.Error = "Failed to initialize the first font face.";
            return result;
        }

        for (std::size_t i = 0; i < FontAtlasData::GlyphCount; ++i)
        {
            const int codepoint = static_cast<int>(FontAtlasData::FirstCodepoint + i);
            if (stbtt_FindGlyphIndex(&font, codepoint) == 0)
            {
                result.Error = "Font does not contain every printable ASCII glyph.";
                return result;
            }
        }

        const float scale = stbtt_ScaleForPixelHeight(&font, pixelHeight);
        if (!std::isfinite(scale) || scale <= 0.0f)
        {
            result.Error = "Font has invalid vertical metrics.";
            return result;
        }

        std::vector<std::uint8_t> pixels( static_cast<std::size_t>(atlasWidth) * atlasHeight, 0);
        std::array<stbtt_packedchar, FontAtlasData::GlyphCount> packedGlyphs{};
        stbtt_pack_context pack{};
        if (!stbtt_PackBegin(&pack, pixels.data(), atlasWidth, atlasHeight, 0, padding, nullptr))
        {
            result.Error = "Failed to initialize font atlas packing.";
            return result;
        }

        const int packed = stbtt_PackFontRange(&pack, fontBytes.data(), 0, pixelHeight, static_cast<int>(FontAtlasData::FirstCodepoint), static_cast<int>(FontAtlasData::GlyphCount), packedGlyphs.data());
        stbtt_PackEnd(&pack);

        if (!packed)
        {
            result.Error = "Failed to pack all glyphs into the 512x512 font atlas.";
            return result;
        }

        int ascent = 0;
        int descent = 0;
        int lineGap = 0;
        stbtt_GetFontVMetrics(&font, &ascent, &descent, &lineGap);

        result.Width = atlasWidth;
        result.Height = atlasHeight;
        result.PixelHeight = pixelHeight;
        result.Ascent = static_cast<float>(ascent) * scale;
        result.Descent = static_cast<float>(descent) * scale;
        result.LineGap = static_cast<float>(lineGap) * scale;
        result.LineHeight = result.Ascent - result.Descent + result.LineGap;

        result.Pixels.swap(pixels);

        for (std::size_t i = 0; i < FontAtlasData::GlyphCount; ++i)
        {
            const auto& packedGlyph = packedGlyphs[i];
            auto& glyph = result.Glyphs[i];

            glyph.AtlasX0 = packedGlyph.x0;
            glyph.AtlasY0 = packedGlyph.y0;
            glyph.AtlasX1 = packedGlyph.x1;
            glyph.AtlasY1 = packedGlyph.y1;

            glyph.OffsetX0 = packedGlyph.xoff;
            glyph.OffsetY0 = packedGlyph.yoff;
            glyph.OffsetX1 = packedGlyph.xoff2;
            glyph.OffsetY1 = packedGlyph.yoff2;
            glyph.AdvanceX = packedGlyph.xadvance;

            const int previous = static_cast<int>(FontAtlasData::FirstCodepoint + i);

            for (std::size_t j = 0; j < FontAtlasData::GlyphCount; ++j)
            {
                const int current = static_cast<int>(FontAtlasData::FirstCodepoint + j);
                result.Kerning[i * FontAtlasData::GlyphCount + j] = static_cast<float>(stbtt_GetCodepointKernAdvance(&font, previous, current)) * scale;
            }
        }

        return result;
    }
}
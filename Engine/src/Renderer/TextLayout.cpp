#include "Quiet/Renderer/TextLayout.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>

#include "Quiet/Assets/FontAtlasData.h"

namespace Quiet::Renderer
{
    TextLayoutResult TextLayout::Build(const Assets::FontAtlasData& font, const std::u32string_view text, const float pixelHeight)
    {
        TextLayoutResult result{};

        if (!font.Succeeded())
        {
            result.Error = "Text layout requires a successful font import.";
            return result;
        }

        if (!std::isfinite(pixelHeight) || pixelHeight <= 0.0f)
        {
            result.Error = "Text layout pixel height must be finite and positive.";
            return result;
        }

        const float scale = pixelHeight / font.PixelHeight;
        const float lineHeight = font.LineHeight * scale;
        const float firstBaseline = font.Ascent * scale;

        if (!std::isfinite(scale) || !std::isfinite(lineHeight) || !std::isfinite(firstBaseline) || scale <= 0.0f || lineHeight <= 0.0f)
        {
            result.Error = "Text layout has invalid scaled font metrics.";
            return result;
        }

        if (text.empty())
        {
            return result;
        }

        result.Quads.reserve(text.size());

        const float inverseAtlasWidth = 1.0f / static_cast<float>(font.Width);
        const float inverseAtlasHeight = 1.0f / static_cast<float>(font.Height);
        const float tabAdvance = font.GetGlyph(U' ').AdvanceX * scale * 4.0f;

        float penX = 0.0f;
        float baselineY = firstBaseline;
        std::size_t lineCount = 1;

        std::uint32_t previousCodepoint = 0;
        bool hasPreviousGlyph = false;

        for (std::size_t i = 0; i < text.size(); ++i)
        {
            const std::uint32_t codepoint = text[i];
            if (codepoint == U'\r' || codepoint == U'\n')
            {
                if (codepoint == U'\r' && i + 1 < text.size() && text[i + 1] == U'\n')
                {
                    ++i;
                }

                result.Size.x = std::max(result.Size.x, penX);
                penX = 0.0f;
                baselineY += lineHeight;
                ++lineCount;
                hasPreviousGlyph = false;
                continue;
            }

            if (codepoint == U'\t')
            {
                penX += tabAdvance;
                hasPreviousGlyph = false;
                continue;
            }

            if (hasPreviousGlyph)
            {
                penX += font.GetKerningAdjustment(previousCodepoint, codepoint) * scale;
            }

            const Assets::FontGlyph& glyph = font.GetGlyph(codepoint);

            if (glyph.AtlasX1 > glyph.AtlasX0 && glyph.AtlasY1 > glyph.AtlasY0)
            {
                TextGlyphQuad quad{};

                quad.Min = glm::vec2(penX + glyph.OffsetX0 * scale, baselineY + glyph.OffsetY0 * scale);
                quad.Max = glm::vec2(penX + glyph.OffsetX1 * scale, baselineY + glyph.OffsetY1 * scale);
                quad.UVMin = glm::vec2(static_cast<float>(glyph.AtlasX0) * inverseAtlasWidth, static_cast<float>(glyph.AtlasY0) * inverseAtlasHeight);
                quad.UVMax = glm::vec2(static_cast<float>(glyph.AtlasX1) * inverseAtlasWidth, static_cast<float>(glyph.AtlasY1) * inverseAtlasHeight);
                result.Quads.push_back(quad);
            }

            penX += glyph.AdvanceX * scale;
            previousCodepoint = codepoint;
            hasPreviousGlyph = true;
        }

        result.Size.x = std::max(result.Size.x, penX);
        result.Size.y = lineHeight * static_cast<float>(lineCount);

        return result;
    }
}
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace Quiet::Assets
{
    /**
     * @brief Describes one rasterized glyph and its placement metrics.
     * @ingroup quiet_assets
     *
     * Atlas bounds use texel coordinates with exclusive maximum bounds.
     * Offsets use pixels relative to the pen position on the baseline.
     * Positive X points right; positive Y points down.
     */
    struct FontGlyph
    {
        std::uint32_t AtlasX0 = 0;
        std::uint32_t AtlasY0 = 0;
        std::uint32_t AtlasX1 = 0;
        std::uint32_t AtlasY1 = 0;

        float OffsetX0 = 0.0f;
        float OffsetY0 = 0.0f;
        float OffsetX1 = 0.0f;
        float OffsetY1 = 0.0f;

        float AdvanceX = 0.0f;
    };

    /**
     * @brief Owns a printable-ASCII coverage atlas and pixel-space metrics.
     * @ingroup quiet_assets
     *
     * Pixels contains one linear coverage byte per texel, stored row-major.
     * Glyphs and kerning use codepoint order beginning at FirstCodepoint.
     * No data borrows memory from the font importer or its source file.
     */
    struct FontAtlasData
    {
        static constexpr std::uint32_t FirstCodepoint = 32;
        static constexpr std::size_t GlyphCount = 95;
        static constexpr std::uint32_t FallbackCodepoint = '?';

        std::uint32_t Width = 0;
        std::uint32_t Height = 0;

        float PixelHeight = 0.0f;
        float Ascent = 0.0f;
        float Descent = 0.0f;
        float LineGap = 0.0f;
        float LineHeight = 0.0f;

        std::vector<std::uint8_t> Pixels;
        std::array<FontGlyph, GlyphCount> Glyphs{};

        // Row = preceding glyph; column = following glyph.
        std::array<float, GlyphCount * GlyphCount> Kerning{};

        std::string Error;

        [[nodiscard]] bool Succeeded() const
        {
            return Error.empty() &&
                   Width > 0 &&
                   Height > 0 &&
                   Pixels.size() == static_cast<std::size_t>(Width) * Height;
        }

        /**
         * @brief Returns a glyph, substituting '?' outside the atlas range.
         * @note Control characters must be handled by text layout separately.
         */
        [[nodiscard]] const FontGlyph& GetGlyph(std::uint32_t codepoint) const
        {
            if (codepoint < FirstCodepoint || codepoint >= FirstCodepoint + GlyphCount)
            {
                codepoint = FallbackCodepoint;
            }
            return Glyphs[codepoint - FirstCodepoint];
        }

        /**
         * @brief Returns the horizontal kerning adjustment in pixels.
         * @note Unsupported codepoints use the fallback glyph's adjustment.
         */
        [[nodiscard]] float GetKerningAdjustment(std::uint32_t previous, std::uint32_t current) const
        {
            if (previous < FirstCodepoint || previous >= FirstCodepoint + GlyphCount)
            {
                previous = FallbackCodepoint;
            }

            if (current < FirstCodepoint || current >= FirstCodepoint + GlyphCount)
            {
                current = FallbackCodepoint;
            }

            const std::size_t row = previous - FirstCodepoint;
            const std::size_t column = current - FirstCodepoint;
            return Kerning[row * GlyphCount + column];
        }
    };
}
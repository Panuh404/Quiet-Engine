#pragma once

#include <string>
#include <string_view>
#include <vector>

#include <glm/vec2.hpp>

namespace Quiet::Assets
{
    struct FontAtlasData;
}

namespace Quiet::Renderer
{
    /**
     * @brief Describes a positioned glyph rectangle and its atlas UVs.
     * @ingroup quiet_renderer
     *
     * Positions use pixels relative to the layout's top-left origin.
     * Positive X points right; positive Y points down.
     */
    struct TextGlyphQuad
    {
        glm::vec2 Min{0.0f};
        glm::vec2 Max{0.0f};

        glm::vec2 UVMin{0.0f};
        glm::vec2 UVMax{0.0f};
    };

    /**
     * @brief Owns text geometry and logical line-box dimensions.
     * @ingroup quiet_renderer
     *
     * Size includes trailing spaces and empty lines.
     * Visible glyphs can extend outside these logical dimensions.
     */
    struct TextLayoutResult
    {
        std::vector<TextGlyphQuad> Quads;
        glm::vec2 Size{0.0f};

        std::string Error;

        [[nodiscard]] bool Succeeded() const
        {
            return Error.empty();
        }
    };

    /**
     * @brief Positions glyphs without accessing rendering resources.
     * @ingroup quiet_renderer
     */
    class TextLayout
    {
    public:
        /**
         * @brief Builds glyph rectangles at the requested pixel height.
         * @param font Unmodified data from a successful FontImporter result.
         * @param text Unicode codepoints; unsupported glyphs use '?'.
         * @param pixelHeight Requested ascender-to-descender height.
         * @return Owned layout data, or a result containing Error.
         * @note Empty text produces no quads and zero dimensions.
         * @note CRLF is one line break. CR and LF also work independently.
         * @note Tabs advance by four spaces, not to tab stops.
         * @note Other unsupported control characters use the fallback glyph.
         * @note Inputs are borrowed only during this call.
         *       Allocation exceptions propagate.
         */
        [[nodiscard]] static TextLayoutResult Build(const Assets::FontAtlasData& font, std::u32string_view text, float pixelHeight);
    };
}
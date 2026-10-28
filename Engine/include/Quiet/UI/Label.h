#pragma once

#include <string>

#include <glm/vec4.hpp>

#include "Quiet/UI/TextStyle.h"
#include "Quiet/UI/Widget.h"

namespace Quiet::UI
{
    /**
     * @brief Measures and paints UTF-8 content using the default runtime font.
     * @ingroup quiet_renderer
     *
     * Authored size supplies minimum preferred dimensions.
     * Text begins at the arranged surface-space top-left.
     * Owns text appearance independently of content.
     * Measurement does not wrap text; arrangement does not scale it.
     * Clipping follows the base Widget contract.
     * Empty content emits no text command.
     *
     * Retains no font or GPU resources.
     */
    class Label final : public Widget
    {
    public:
        /**
         * @brief Validates and copies text appearance.
         * @return False for nonfinite/nonpositive height or nonfinite color,
         *         preserving the current style.
         * @note Finite color channels are clamped to [0, 1].
         *       Height changes invalidate layout; color-only changes
         *       invalidate paint. Unchanged effective values do neither.
         */
        bool SetStyle(TextStyle style);

        /**
         * @brief Returns this label's owned style.
         * @note Borrowed for the Label's lifetime; style setters can
         *       change its contents.
         */
        [[nodiscard]] const TextStyle& GetStyle() const noexcept { return m_Style; }

        /**
         * @brief Replaces the owned text content.
         * @note UTF-8 validation occurs during text layout.
         */
        void SetContent(std::string content);
        [[nodiscard]] const std::string& GetContent() const noexcept { return m_Content; }

        /**
         * @brief Sets the requested ascender-to-descender pixel height.
         * @return False for nonfinite or nonpositive input, preserving
         *         the previous height; otherwise true.
         */
        bool SetPixelHeight(float pixelHeight);
        [[nodiscard]] float GetPixelHeight() const noexcept { return m_Style.PixelHeight; }

        /**
         * @brief Sets straight-alpha linear RGBA.
         * @return False for nonfinite input, preserving the previous color.
         *         Finite channels are clamped to [0, 1].
         */
        bool SetColor(const glm::vec4& color);
        [[nodiscard]] const glm::vec4& GetColor() const noexcept { return m_Style.Color; }

    protected:
        /**
         * @brief Measures text using the borrowed default font and authored minimum size.
         * @note Malformed text contributes no intrinsic size.
         * @throws std::runtime_error When nonempty content has no default font context.
         */
        glm::vec2 OnMeasure(const LayoutContext& context) const override;

        void OnPaint(PaintList& commands) const override;

    private:
        std::string m_Content;
        TextStyle m_Style{};
    };
}
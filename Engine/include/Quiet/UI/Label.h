#pragma once

#include <string>

#include <glm/vec4.hpp>

#include "Quiet/UI/Widget.h"

namespace Quiet::UI
{
    /**
     * @brief Measures and paints UTF-8 content using the default runtime font.
     * @ingroup quiet_renderer
     *
     * Authored size supplies minimum preferred dimensions.
     * Text begins at the arranged surface-space top-left.
     * Measurement does not wrap text; arrangement does not scale or clip it.
     * Empty content emits no text command.
     *
     * Retains no font or GPU resources.
     */
    class Label final : public Widget
    {
    public:
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
        [[nodiscard]] float GetPixelHeight() const noexcept { return m_PixelHeight; }

        /**
         * @brief Sets straight-alpha linear RGBA.
         * @return False for nonfinite input, preserving the previous color.
         *         Finite channels are clamped to [0, 1].
         */
        bool SetColor(const glm::vec4& color);
        [[nodiscard]] const glm::vec4& GetColor() const noexcept { return m_Color; }

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
        float m_PixelHeight = 32.0f;
        glm::vec4 m_Color{1.0f};
    };
}
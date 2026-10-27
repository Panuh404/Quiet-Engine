#pragma once

#include <glm/vec4.hpp>

#include "Quiet/UI/Widget.h"

namespace Quiet::UI
{
    /**
     * @brief Measures and horizontally arranges child widgets.
     * @ingroup quiet_renderer
     *
     * Preferred size includes child desired sizes, padding, and spacing.
     * Authored size supplies minimum preferred dimensions.
     * Children retain desired widths and use vertical alignment
     * within the allocated height.
     *
     * Preserves authored child geometry.
     * Clipping follows the base Widget contract.
     */
    class HorizontalBox final : public Widget
    {
    public:
        /**
         * @brief Sets left, top, right, and bottom padding in XYZW.
         * @return False for nonfinite or negative input.
         */
        bool SetPadding(const glm::vec4& padding);
        [[nodiscard]] const glm::vec4& GetPadding() const noexcept { return m_Padding; }

        /**
         * @brief Sets the nonnegative pixel gap between children.
         * @return False for nonfinite or negative input.
         */
        bool SetSpacing(float spacing);
        [[nodiscard]] float GetSpacing() const noexcept { return m_Spacing; }

    protected:
        /**
         * @brief Measures children, padding, and gaps.
         * @note Child desired sizes are already available.
         */
        glm::vec2 OnMeasure(const LayoutContext& context) const override;

        /**
         * @brief Arranges children from left to right in insertion order.
         * @pre Calculated positions must remain finite.
         */
        void OnArrangeChildren() override;

    private:
        glm::vec4 m_Padding{0.0f};
        float m_Spacing = 0.0f;
    };
}
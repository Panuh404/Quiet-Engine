#pragma once

#include <glm/vec4.hpp>

#include "Quiet/UI/Widget.h"

namespace Quiet::UI
{
    /**
     * @brief Paints a solid background over its explicit widget bounds.
     * @ingroup quiet_renderer
     *
     * Children paint afterward using the base Widget placement rules.
     * Descendants can overflow unless clipping is enabled on this widget
     * or an ancestor.
     */
    class Panel final : public Widget
    {
    public:
        /**
         * @brief Sets straight-alpha linear RGBA.
         * @return False for nonfinite input, preserving the previous color.
         *         Finite channels are clamped to [0, 1].
         */
        bool SetColor(const glm::vec4& color);
        [[nodiscard]] const glm::vec4& GetColor() const noexcept { return m_Color; }

    protected:
        /**
         * @brief Appends a solid rectangle when both dimensions are positive.
         * @pre Surface-space bounds and their maximum coordinates must
         *      remain representable as finite floats.
         */
        void OnPaint(PaintList& commands) const override;

    private:
        glm::vec4 m_Color{0.03f, 0.12f, 0.25f, 0.8f};
    };
}
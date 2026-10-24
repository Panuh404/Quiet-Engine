#pragma once

#include "Quiet/UI/Widget.h"

namespace Quiet::UI
{
    /**
     * @brief Paints a button background using retained pointer-hover state.
     * @ingroup quiet_renderer
     *
     * Participates in hit testing by default.
     * Child widgets provide content and paint above the background.
     * Uses explicit bounds without automatic layout or child clipping.
     *
     * Uses retained hover and primary-button press state for its background.
     * Activation and configurable styling are not implemented yet.
     */
    class Button final : public Widget
    {
    public:
        Button();

    protected:
        /**
         * @brief Emits the normal, hovered, or pressed background rectangle.
         * @pre Surface bounds and their maximum coordinates must remain finite.
         */
        void OnPaint(PaintList& commands) const override;
    };
}
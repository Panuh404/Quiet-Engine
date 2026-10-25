#pragma once

#include <memory>
#include <functional>

#include "Quiet/UI/Widget.h"

namespace Quiet::UI
{
    /**
     * @brief Paints pointer states and invokes configured activation behavior.
     * @ingroup quiet_renderer
     *
     * Participates in hit testing by default.
     * Owns its callback and child content, without GPU resources.
     * Uses explicit bounds without automatic layout or child clipping.
     * Configurable styling is not implemented yet.
     */
    class Button final : public Widget
    {
    public:
        Button();

        /**
         * @brief Replaces activation behavior; an empty callback clears it.
         * @note Captured references must remain valid when invoked.
         */
        void SetOnClick(std::function<void()> callback);

        /**
         * @brief Invokes the configured callback while retaining its lifetime.
         * @note Call outside hierarchy traversal. Exceptions propagate.
         *       Mutable callable state persists between activations.
         *       The callback may replace itself or modify the widget tree.
         */
        void Activate() override;

    protected:
        /**
         * @brief Emits the normal, hovered, or pressed background rectangle.
         * @pre Surface bounds and their maximum coordinates must remain finite.
         */
        void OnPaint(PaintList& commands) const override;

    private:
        std::shared_ptr<std::function<void()>> m_OnClick;
    };
}
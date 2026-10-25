#pragma once

#include <memory>
#include <functional>

#include <glm/vec4.hpp>
#include "Quiet/UI/Widget.h"

namespace Quiet::UI
{
    /**
     * @brief Paints pointer states and invokes configured activation behavior.
     * @ingroup quiet_renderer
     *
     * Participates in hit testing by default.
     * Owns its callback and child content, without GPU resources.
     * Measures child content with padding, using authored size as a minimum.
     * Arranges children over the padded content rectangle without clipping.
     * Configurable styling is not implemented yet.
     */
    class Button final : public Widget
    {
    public:
        Button();

        /**
         * @brief Sets nonnegative left, top, right, and bottom padding in XYZW.
         * @return False for nonfinite or negative input.
         */
        bool SetPadding(const glm::vec4& padding);
        [[nodiscard]] const glm::vec4& GetPadding() const noexcept { return m_Padding; }

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
         * @brief Measures child content plus padding, using authored size as a minimum.
         */
        glm::vec2 OnMeasure(const LayoutContext& context) const override;

        /**
         * @brief Assigns children the padded content rectangle.
         * @note Multiple children occupy the same rectangle.
         */
        void OnArrangeChildren() override;

        /**
         * @brief Emits the normal, hovered, or pressed background rectangle.
         * @pre Surface bounds and their maximum coordinates must remain finite.
         */
        void OnPaint(PaintList& commands) const override;

    private:
        glm::vec4 m_Padding{16.0f};
        std::shared_ptr<std::function<void()>> m_OnClick;
    };
}
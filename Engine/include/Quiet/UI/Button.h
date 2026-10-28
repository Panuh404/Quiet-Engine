#pragma once

#include <memory>
#include <functional>

#include <glm/vec4.hpp>
#include "Quiet/UI/ButtonStyle.h"
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
     * Arranges children within the padded content rectangle using alignment.
     * Clipping follows the base Widget contract.
     * Owns a validated ButtonStyle value independently of its callback.
     */
    class Button final : public Widget
    {
    public:
        Button();

        /**
         * @brief Validates and copies button appearance.
         * @return False for nonfinite colors or nonfinite/negative padding,
         *         preserving the current style.
         * @note Finite color channels are clamped to [0, 1].
         *       Padding changes invalidate layout; color-only changes
         *       invalidate paint. Unchanged effective values do neither.
         */
        bool SetStyle(ButtonStyle style);

        /**
         * @brief Returns this button's owned style.
         * @note The reference is borrowed for the Button's lifetime.
         *       SetStyle() and SetPadding() can change its contents.
         */
        [[nodiscard]] const ButtonStyle& GetStyle() const noexcept { return m_Style; }

        /**
         * @brief Sets nonnegative left, top, right, and bottom padding in XYZW.
         * @return False for nonfinite or negative input.
         */
        bool SetPadding(const glm::vec4& padding);
        [[nodiscard]] const glm::vec4& GetPadding() const noexcept { return m_Style.Padding; }

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
         * @note Effectively disabled buttons do not invoke their callback.
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
         * @brief Emits the normal, hovered, pressed, or disabled background.
         */
        void OnPaint(PaintList& commands) const override;

    private:
        ButtonStyle m_Style{};
        std::shared_ptr<std::function<void()>> m_OnClick;
    };
}
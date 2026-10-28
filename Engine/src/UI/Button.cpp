#include "Quiet/UI/Button.h"

#include <algorithm>
#include <cmath>
#include <utility>
#include <array>

#include <glm/common.hpp>

namespace Quiet::UI
{
    Button::Button()
    {
        SetHitTestMode(HitTestMode::SelfAndChildren);
    }

    bool Button::SetStyle(ButtonStyle style)
    {
        for (int component = 0; component < 4; ++component)
        {
            if (!std::isfinite(style.Padding[component]) || style.Padding[component] < 0.0f) return false;
        }

        const std::array colors
        {
            &style.NormalColor,
            &style.HoveredColor,
            &style.PressedColor,
            &style.DisabledColor
        };

        for (glm::vec4* color : colors)
        {
            for (int component = 0; component < 4; ++component)
            {
                if (!std::isfinite((*color)[component])) return false;
            }

            *color = glm::clamp(*color, glm::vec4(0.0f), glm::vec4(1.0f));
        }

        const bool layoutChanged = m_Style.Padding != style.Padding;
        const bool colorsChanged =
            m_Style.NormalColor != style.NormalColor ||
            m_Style.HoveredColor != style.HoveredColor ||
            m_Style.PressedColor != style.PressedColor ||
            m_Style.DisabledColor != style.DisabledColor;

        if (!layoutChanged && !colorsChanged) return true;

        m_Style = style;
        Invalidate(layoutChanged ? InvalidationReason::Layout : InvalidationReason::Paint);
        return true;
    }

    void Button::SetOnClick(std::function<void()> callback)
    {
        if (callback)
        {
            m_OnClick = std::make_shared<std::function<void()>>(std::move(callback));
        }
        else
        {
            m_OnClick.reset();
        }
    }

    void Button::Activate()
    {
        if (!IsEffectivelyEnabled()) return;

        const auto callback = m_OnClick;
        if (callback)
        {
            (*callback)();
        }
    }

    bool Button::SetPadding(const glm::vec4& padding)
    {
        ButtonStyle style = m_Style;
        style.Padding = padding;
        return SetStyle(style);
    }

    glm::vec2 Button::OnMeasure(const LayoutContext&) const
    {
        glm::vec2 contentSize{0.0f};

        for (std::size_t index = 0; index < GetChildCount(); ++index)
        {
            const glm::vec2& childSize = GetChild(index).GetDesiredSize();

            contentSize.x = std::max(contentSize.x, childSize.x);
            contentSize.y = std::max(contentSize.y, childSize.y);
        }

        contentSize += glm::vec2(
            m_Style.Padding.x + m_Style.Padding.z,
            m_Style.Padding.y + m_Style.Padding.w);

        const glm::vec2& requestedSize = GetLocalBounds().Size;

        return glm::vec2(
            std::max(requestedSize.x, contentSize.x),
            std::max(requestedSize.y, contentSize.y));
    }

    void Button::OnArrangeChildren()
    {
        const glm::vec2 allocatedSize = GetSurfaceBounds().Size;

        const glm::vec2 contentSize(
            std::max(0.0f, allocatedSize.x - m_Style.Padding.x - m_Style.Padding.z),
            std::max(0.0f, allocatedSize.y - m_Style.Padding.y - m_Style.Padding.w));

        for (std::size_t index = 0; index < GetChildCount(); ++index)
        {
            Widget& child = GetChild(index);
            const glm::vec2& desiredSize = child.GetDesiredSize();

            glm::vec2 position(m_Style.Padding.x, m_Style.Padding.y);
            glm::vec2 size = contentSize;

            for (int axis = 0; axis < 2; ++axis)
            {
                const Alignment alignment = axis == 0
                    ? child.GetHorizontalAlignment()
                    : child.GetVerticalAlignment();

                if (alignment == Alignment::Stretch)
                {
                    continue;
                }

                size[axis] = desiredSize[axis];
                const float remaining = contentSize[axis] - size[axis];

                if (alignment == Alignment::Center)
                {
                    position[axis] += remaining * 0.5f;
                }
                else if (alignment == Alignment::End)
                {
                    position[axis] += remaining;
                }
            }

            child.SetArrangedBounds({position, size});
        }
    }

    void Button::OnPaint(PaintList& commands) const
    {
        const Rect bounds = GetSurfaceBounds();
        if (bounds.Size.x <= 0.0f || bounds.Size.y <= 0.0f) return;

        glm::vec4 color = m_Style.NormalColor;

        if (!IsEffectivelyEnabled())
        {
            color = m_Style.DisabledColor;
        }
        else if (IsPointerHovered())
        {
            color = IsPointerPressed()
                ? m_Style.PressedColor
                : m_Style.HoveredColor;
        }

        commands.emplace_back(SolidRectCommand
        {
            bounds.Position,
            bounds.Position + bounds.Size,
            color
        });
    }
}
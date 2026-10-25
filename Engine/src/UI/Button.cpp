#include "Quiet/UI/Button.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace Quiet::UI
{
    Button::Button()
    {
        SetHitTestMode(HitTestMode::SelfAndChildren);
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
        const auto callback = m_OnClick;
        if (callback)
        {
            (*callback)();
        }
    }

    bool Button::SetPadding(const glm::vec4& padding)
    {
        if (!std::isfinite(padding.x) ||
            !std::isfinite(padding.y) ||
            !std::isfinite(padding.z) ||
            !std::isfinite(padding.w) ||
            padding.x < 0.0f ||
            padding.y < 0.0f ||
            padding.z < 0.0f ||
            padding.w < 0.0f)
        {
            return false;
        }

        if (m_Padding == padding)
        {
            return true;
        }

        m_Padding = padding;
        Invalidate(InvalidationReason::Layout);
        return true;
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
            m_Padding.x + m_Padding.z,
            m_Padding.y + m_Padding.w);

        const glm::vec2& requestedSize = GetLocalBounds().Size;

        return glm::vec2(
            std::max(requestedSize.x, contentSize.x),
            std::max(requestedSize.y, contentSize.y));
    }

    void Button::OnArrangeChildren()
    {
        const glm::vec2 allocatedSize = GetSurfaceBounds().Size;
        const glm::vec2 contentSize(
            std::max(0.0f, allocatedSize.x - m_Padding.x - m_Padding.z),
            std::max(0.0f, allocatedSize.y - m_Padding.y - m_Padding.w));

        for (std::size_t index = 0; index < GetChildCount(); ++index)
        {
            GetChild(index).SetArrangedBounds({ {m_Padding.x, m_Padding.y}, contentSize });
        }
    }

    void Button::OnPaint(PaintList& commands) const
    {
        const Rect bounds = GetSurfaceBounds();
        if (bounds.Size.x <= 0.0f || bounds.Size.y <= 0.0f)
        {
            return;
        }

        glm::vec4 color(0.03f, 0.12f, 0.25f, 0.8f);

        if (IsPointerHovered())
        {
            color = IsPointerPressed()
                ? glm::vec4(0.02f, 0.08f, 0.18f, 1.0f)
                : glm::vec4(0.08f, 0.22f, 0.42f, 1.0f);
        }

        commands.emplace_back(SolidRectCommand
        {
            bounds.Position,
            bounds.Position + bounds.Size,
            color
        });
    }
}
#include "Quiet/UI/VerticalBox.h"

#include <algorithm>
#include <cmath>

namespace Quiet::UI
{
    bool VerticalBox::SetPadding(const glm::vec4& padding)
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

        if (m_Padding == padding) return true;
        m_Padding = padding;
        Invalidate(InvalidationReason::Layout);
        return true;
    }

    bool VerticalBox::SetSpacing(const float spacing)
    {
        if (!std::isfinite(spacing) || spacing < 0.0f) return false;
        if (m_Spacing == spacing) return true;

        m_Spacing = spacing;
        Invalidate(InvalidationReason::Layout);
        return true;
    }

    glm::vec2 VerticalBox::OnMeasure(const LayoutContext&) const
    {
        float contentWidth = 0.0f;
        float contentHeight = 0.0f;

        for (std::size_t index = 0; index < GetChildCount(); ++index)
        {
            const glm::vec2& childSize = GetChild(index).GetDesiredSize();

            contentWidth = std::max(contentWidth, childSize.x);
            if (index > 0)
            {
                contentHeight += m_Spacing;
            }
            contentHeight += childSize.y;
        }

        const glm::vec2 contentSize(
            contentWidth + m_Padding.x + m_Padding.z,
            contentHeight + m_Padding.y + m_Padding.w);

        const glm::vec2& requestedSize = GetLocalBounds().Size;

        return glm::vec2(
            std::max(requestedSize.x, contentSize.x),
            std::max(requestedSize.y, contentSize.y));
    }

    void VerticalBox::OnArrangeChildren()
    {
        const float width = std::max(0.0f, GetSurfaceBounds().Size.x - m_Padding.x - m_Padding.z);
        float nextY = m_Padding.y;

        for (std::size_t index = 0; index < GetChildCount(); ++index)
        {
            Widget& child = GetChild(index);
            const float height = child.GetDesiredSize().y;

            child.SetArrangedBounds(
            {
                {m_Padding.x, nextY},
                {width, height}
            });

            nextY += height + m_Spacing;
        }
    }
}
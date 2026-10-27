#include "Quiet/UI/HorizontalBox.h"

#include <algorithm>
#include <cmath>

namespace Quiet::UI
{
    bool HorizontalBox::SetPadding(const glm::vec4& padding)
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

    bool HorizontalBox::SetSpacing(const float spacing)
    {
        if (!std::isfinite(spacing) || spacing < 0.0f) return false;
        if (m_Spacing == spacing) return true;

        m_Spacing = spacing;
        Invalidate(InvalidationReason::Layout);
        return true;
    }

    glm::vec2 HorizontalBox::OnMeasure(const LayoutContext&) const
    {
        float contentWidth = 0.0f;
        float contentHeight = 0.0f;

        for (std::size_t index = 0; index < GetChildCount(); ++index)
        {
            const glm::vec2& childSize = GetChild(index).GetDesiredSize();
            if (index > 0)
            {
                contentWidth += m_Spacing;
            }

            contentWidth += childSize.x;
            contentHeight = std::max(contentHeight, childSize.y);
        }

        const glm::vec2 contentSize(
            contentWidth + m_Padding.x + m_Padding.z,
            contentHeight + m_Padding.y + m_Padding.w);

        const glm::vec2& requestedSize = GetLocalBounds().Size;
        return glm::vec2(
            std::max(requestedSize.x, contentSize.x),
            std::max(requestedSize.y, contentSize.y));
    }

    void HorizontalBox::OnArrangeChildren()
    {
        const float availableHeight = std::max(0.0f, GetSurfaceBounds().Size.y - m_Padding.y - m_Padding.w);
        float nextX = m_Padding.x;

        for (std::size_t index = 0; index < GetChildCount(); ++index)
        {
            Widget& child = GetChild(index);
            const glm::vec2& desiredSize = child.GetDesiredSize();

            float y = m_Padding.y;
            float height = availableHeight;

            switch (child.GetVerticalAlignment())
            {
                case Alignment::Start:
                    height = desiredSize.y;
                    break;

                case Alignment::Center:
                    height = desiredSize.y;
                    y += (availableHeight - height) * 0.5f;
                    break;

                case Alignment::End:
                    height = desiredSize.y;
                    y += availableHeight - height;
                    break;

                case Alignment::Stretch:
                    break;
            }

            child.SetArrangedBounds(
            {
                {nextX, y},
                {desiredSize.x, height}
            });

            nextX += desiredSize.x + m_Spacing;
        }
    }
}
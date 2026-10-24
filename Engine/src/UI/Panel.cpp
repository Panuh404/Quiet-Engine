#include "Quiet/UI/Panel.h"

#include <cmath>

#include <glm/common.hpp>

namespace Quiet::UI
{
    bool Panel::SetColor(const glm::vec4& color)
    {
        if (!std::isfinite(color.r) ||
            !std::isfinite(color.g) ||
            !std::isfinite(color.b) ||
            !std::isfinite(color.a))
        {
            return false;
        }

        const glm::vec4 clampedColor = glm::clamp(color, glm::vec4(0.0f), glm::vec4(1.0f));
        if (m_Color == clampedColor) return true;

        m_Color = clampedColor;
        Invalidate(InvalidationReason::Paint);
        return true;
    }

    void Panel::OnPaint(PaintList& commands) const
    {
        const Rect bounds = GetSurfaceBounds();
        if (bounds.Size.x <= 0.0f || bounds.Size.y <= 0.0f) return;

        commands.emplace_back(SolidRectCommand
        {
            bounds.Position,
            bounds.Position + bounds.Size,
            m_Color
        });
    }
}
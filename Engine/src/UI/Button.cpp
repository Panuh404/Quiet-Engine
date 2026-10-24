#include "Quiet/UI/Button.h"

namespace Quiet::UI
{
    Button::Button()
    {
        SetHitTestMode(HitTestMode::SelfAndChildren);
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
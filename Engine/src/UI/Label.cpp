#include "Quiet/UI/Label.h"

#include <cmath>
#include <utility>
#include <stdexcept>

#include "Quiet/Renderer/TextLayout.h"

#include <glm/common.hpp>

namespace Quiet::UI
{
    bool Label::SetStyle(TextStyle style)
    {
        if (!std::isfinite(style.PixelHeight) ||
            style.PixelHeight <= 0.0f ||
            !std::isfinite(style.Color.r) ||
            !std::isfinite(style.Color.g) ||
            !std::isfinite(style.Color.b) ||
            !std::isfinite(style.Color.a))
        {
            return false;
        }

        style.Color = glm::clamp(style.Color, glm::vec4(0.0f), glm::vec4(1.0f));

        const bool layoutChanged = m_Style.PixelHeight != style.PixelHeight;
        const bool colorChanged = m_Style.Color != style.Color;
        if (!layoutChanged && !colorChanged) return true;

        m_Style = style;
        Invalidate(layoutChanged ? InvalidationReason::Layout : InvalidationReason::Paint);
        return true;
    }

    void Label::SetContent(std::string content)
    {
        if (m_Content == content) return;
        m_Content = std::move(content);

        Invalidate(InvalidationReason::Layout);
    }
    bool Label::SetPixelHeight(const float pixelHeight)
    {
        TextStyle style = m_Style;
        style.PixelHeight = pixelHeight;
        return SetStyle(style);
    }

    bool Label::SetColor(const glm::vec4& color)
    {
        TextStyle style = m_Style;
        style.Color = color;
        return SetStyle(style);
    }

    glm::vec2 Label::OnMeasure(const LayoutContext& context) const
    {
        const glm::vec2& requestedSize = GetLocalBounds().Size;
        if (m_Content.empty())
        {
            return requestedSize;
        }

        if (!context.DefaultFontAtlas)
        {
            throw std::runtime_error("Label measurement requires a default font atlas.");
        }

        const Renderer::TextLayoutResult layout = Renderer::TextLayout::BuildUTF8(*context.DefaultFontAtlas, m_Content, m_Style.PixelHeight);
        if (!layout.Succeeded())
        {
            return requestedSize;
        }

        return glm::max(requestedSize, layout.Size);
    }

    void Label::OnPaint(PaintList& commands) const
    {
        if (m_Content.empty()) return;

        const Rect bounds = GetSurfaceBounds();
        if (bounds.Size.x <= 0.0f || bounds.Size.y <= 0.0f) return;

        commands.emplace_back(TextCommand
        {
            m_Content,
            bounds.Position,
            m_Style.PixelHeight,
            m_Style.Color
        });
    }
}
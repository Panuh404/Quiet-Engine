#include "Quiet/UI/Label.h"

#include <cmath>
#include <utility>
#include <stdexcept>

#include "Quiet/Renderer/TextLayout.h"

#include <glm/common.hpp>

namespace Quiet::UI
{
    void Label::SetContent(std::string content)
    {
        if (m_Content == content) return;
        m_Content = std::move(content);

        Invalidate(InvalidationReason::Layout);
    }
    bool Label::SetPixelHeight(const float pixelHeight)
    {
        if (!std::isfinite(pixelHeight) || pixelHeight <= 0.0f) return false;
        if (m_PixelHeight == pixelHeight) return true;

        m_PixelHeight = pixelHeight;
        Invalidate(InvalidationReason::Layout);
        return true;
    }

    bool Label::SetColor(const glm::vec4& color)
    {
        if (!std::isfinite(color.r) ||
            !std::isfinite(color.g) ||
            !std::isfinite(color.b) ||
            !std::isfinite(color.a))
            return false;

        const glm::vec4 clampedColor = glm::clamp(color, glm::vec4(0.0f), glm::vec4(1.0f));
        if (m_Color == clampedColor) return true;
        m_Color = clampedColor;

        Invalidate(InvalidationReason::Paint);
        return true;
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

        const Renderer::TextLayoutResult layout = Renderer::TextLayout::BuildUTF8(*context.DefaultFontAtlas, m_Content, m_PixelHeight);
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
            m_PixelHeight,
            m_Color
        });
    }
}
#include "Quiet/UI/Image.h"

#include <cmath>
#include <utility>

#include <glm/common.hpp>

namespace Quiet::UI
{
    void Image::SetTextureID(std::string textureID)
    {
        if (m_TextureID == textureID) return;

        m_TextureID = std::move(textureID);
        Invalidate(InvalidationReason::Paint);
    }

    bool Image::SetTint(const glm::vec4& tint)
    {
        if (!std::isfinite(tint.r) ||
            !std::isfinite(tint.g) ||
            !std::isfinite(tint.b) ||
            !std::isfinite(tint.a))
        {
            return false;
        }

        const glm::vec4 clampedTint = glm::clamp(tint, glm::vec4(0.0f), glm::vec4(1.0f));
        if (m_Tint == clampedTint)
        {
            return true;
        }

        m_Tint = clampedTint;
        Invalidate(InvalidationReason::Paint);
        return true;
    }

    void Image::OnPaint(PaintList& commands) const
    {
        if (m_TextureID.empty()) return;
        const Rect bounds = GetSurfaceBounds();

        if (bounds.Size.x <= 0.0f || bounds.Size.y <= 0.0f)
        {
            return;
        }

        commands.emplace_back(ImageCommand
        {
            bounds.Position,
            bounds.Position + bounds.Size,
            m_TextureID,
            m_Tint
        });
    }
}
#pragma once

#include <cmath>
#include <optional>

#include <glm/vec2.hpp>

namespace Quiet::UI
{
    /**
     * @brief Describes an axis-aligned UI rectangle.
     * @ingroup quiet_renderer
     *
     * Position and Size use pixel units with positive X rightward and Y downward.
     * Retained widget geometry uses logical UI pixels. Rendering commands are
     * converted to target pixels at the Application submission boundary.
     * Position's coordinate space is defined by the containing API.
     * Size is nonnegative; zero means empty, not automatic sizing.
     *
     * Contains no clipping, transform, or rendering state.
     */
    struct Rect
    {
        glm::vec2 Position{0.0f};
        glm::vec2 Size{0.0f};
    };

    /**
     * @brief Converts a displayed-image point into UI surface pixels.
     * @ingroup quiet_renderer
     * @param displayPosition Point in the same coordinate space as displayBounds.
     * @param displayBounds Position and size of the displayed surface image.
     * @param surfaceSize Actual rendered surface dimensions in pixels.
     * @return Converted position, or null for invalid input or a nonfinite result.
     * @note Both coordinate spaces use positive X rightward and Y downward.
     *       Positions outside displayBounds remain outside the surface.
     *       Does not clamp, test hover, or apply an additional DPI scale.
     */
    [[nodiscard]] inline std::optional<glm::vec2> MapPointToSurface(const glm::vec2& displayPosition, const Rect& displayBounds, const glm::vec2& surfaceSize) noexcept
    {
        if (!std::isfinite(displayPosition.x) ||
            !std::isfinite(displayPosition.y) ||
            !std::isfinite(displayBounds.Position.x) ||
            !std::isfinite(displayBounds.Position.y) ||
            !std::isfinite(displayBounds.Size.x) ||
            !std::isfinite(displayBounds.Size.y) ||
            !std::isfinite(surfaceSize.x) ||
            !std::isfinite(surfaceSize.y) ||
            displayBounds.Size.x <= 0.0f ||
            displayBounds.Size.y <= 0.0f ||
            surfaceSize.x <= 0.0f ||
            surfaceSize.y <= 0.0f)
        {
            return std::nullopt;
        }

        const glm::vec2 surfacePosition = ((displayPosition - displayBounds.Position) / displayBounds.Size) * surfaceSize;
        if (!std::isfinite(surfacePosition.x) || !std::isfinite(surfacePosition.y))
        {
            return std::nullopt;
        }

        return surfacePosition;
    }
}
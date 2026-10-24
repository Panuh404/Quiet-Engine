#pragma once

#include <glm/vec2.hpp>

namespace Quiet::UI
{
    /**
     * @brief Describes an axis-aligned UI rectangle.
     * @ingroup quiet_renderer
     *
     * Position and Size use pixels with positive X rightward and Y downward.
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
}
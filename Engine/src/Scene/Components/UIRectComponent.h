#pragma once

#include <glm/vec2.hpp>

namespace Quiet::Scene
{
    /**
     * @brief Stores authored screen-space placement independently of content.
     * @ingroup quiet_scene
     *
     * Offset uses pixels, with positive X rightward and positive Y downward.
     * Anchor and Pivot use normalized coordinates, intended within [0, 1].
     *
     * Contains no resolved rectangle, resource reference, or runtime cache.
     * Text rendering resolves placement against the Scene target extent.
     * Zero Size axes use the text's logical layout dimensions.
     * Positive Size axes define explicit bounds without scaling,
     * wrapping, or clipping the text.
     */
    struct UIRectComponent
    {
        glm::vec2 Offset{24.0f, 24.0f};
        glm::vec2 Size{0.0f};
        glm::vec2 Anchor{0.0f};
        glm::vec2 Pivot{0.0f};
    };
}
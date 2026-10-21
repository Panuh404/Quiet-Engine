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
     * Text rendering resolves placement against the Scene target extent
     * and the text's logical layout dimensions.
     */
    struct UIRectComponent
    {
        glm::vec2 Offset{24.0f, 24.0f};
        glm::vec2 Anchor{0.0f};
        glm::vec2 Pivot{0.0f};
    };
}
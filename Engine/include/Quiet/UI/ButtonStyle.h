#pragma once

#include <glm/vec4.hpp>

namespace Quiet::UI
{
    /**
     * @brief Defines button background colors and content padding.
     * @ingroup quiet_renderer
     *
     * Colors use straight-alpha linear RGBA.
     * Padding uses left, top, right, bottom in XYZW, measured in pixels.
     *
     * Contains values only; owns no widgets, callbacks, or GPU resources.
     * Button::SetStyle() validates and copies these values.
     */
    struct ButtonStyle
    {
        glm::vec4 NormalColor{0.03f, 0.12f, 0.25f, 0.8f};
        glm::vec4 HoveredColor{0.08f, 0.22f, 0.42f, 1.0f};
        glm::vec4 PressedColor{0.02f, 0.08f, 0.18f, 1.0f};
        glm::vec4 DisabledColor{0.08f, 0.08f, 0.1f, 0.8f};

        glm::vec4 Padding{16.0f};
    };
}
#pragma once

#include <glm/vec4.hpp>

namespace Quiet::UI
{
    /**
     * @brief Defines text size and color using the default runtime font.
     * @ingroup quiet_renderer
     *
     * PixelHeight is the requested ascender-to-descender height.
     * Color uses straight-alpha linear RGBA.
     *
     * Contains values only; owns no content, font, or GPU resources.
     * Label::SetStyle() validates and copies these values.
     */
    struct TextStyle
    {
        float PixelHeight = 32.0f;
        glm::vec4 Color{1.0f};
    };
}
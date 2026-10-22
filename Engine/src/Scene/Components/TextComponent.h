#pragma once

#include <string>
#include <glm/vec4.hpp>

namespace Quiet::Scene
{
    /**
     * @brief Stores authored UTF-8 text and appearance on a Scene Actor.
     * @ingroup quiet_scene
     *
     * Contains no resolved font resources or layout cache.
     * Optional UIRectComponent data controls screen-space placement.
     * Font selection currently belongs to the renderer.
     */
    struct TextComponent
    {
        std::string Content = "Text";

        // Requested ascender-to-descender height in pixels.
        float PixelHeight = 32.0f;

        // Straight-alpha linear RGBA.
        glm::vec4 Color{1.0f};

        bool Visible = true;
    };
}
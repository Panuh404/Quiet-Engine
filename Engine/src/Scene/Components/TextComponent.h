#pragma once

#include <string>
#include <glm/vec4.hpp>

namespace Quiet::Scene
{
    /**
     * @brief Stores authored UTF-8 text and appearance.
     * @ingroup quiet_scene
     *
     * Contains no resolved font resources or layout cache.
     * Positioning and font-asset selection are not defined here yet.
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
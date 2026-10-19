#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <cstddef>

namespace Quiet::Renderer::Vulkan
{
    struct CameraUniformData
    {
        alignas(16) glm::mat4 View{1.0f};
        alignas(16) glm::mat4 Projection{1.0f};
    };

    struct StaticMeshPushConstants
    {
        alignas(16) glm::mat4 Model{ 1.0f };

        // rgba = material base color
        alignas(16) glm::vec4 MaterialBaseColor{ 1.0f };

        // xyz = direction from surface toward light, w = intensity
        alignas(16) glm::vec4 LightDirectionIntensity{ -0.35f, 0.85f, -0.35f, 1.0f };

        // rgb = direct light color, w unused
        alignas(16) glm::vec4 LightColor{ 1.0f, 1.0f, 1.0f, 1.0f };

        // rgb = ambient color, w = ambient intensity
        alignas(16) glm::vec4 AmbientColorIntensity{ 1.0f, 1.0f, 1.0f, 0.25f };
    };

    /**
     * @brief Supplies one screen-space glyph to the text vertex shader.
     * @ingroup quiet_renderer
     *
     * Rect contains pixel-space minimum XY and maximum XY.
     * UVRect contains atlas UV minimum XY and maximum XY.
     * Color contains straight-alpha linear RGBA.
     * Viewport contains target width and height in XY; ZW are reserved.
     *
     * Requires positive viewport dimensions and a positive-height
     * Vulkan viewport. This data does not use the Scene camera.
     */
    struct TextPushConstants
     {
         alignas(16) glm::vec4 Rect{0.0f};
         alignas(16) glm::vec4 UVRect{0.0f};
         alignas(16) glm::vec4 Color{1.0f};
         alignas(16) glm::vec4 Viewport{1.0f, 1.0f, 0.0f, 0.0f};
     };

    static_assert(sizeof(TextPushConstants) == 64);
    static_assert(offsetof(TextPushConstants, Rect) == 0);
    static_assert(offsetof(TextPushConstants, UVRect) == 16);
    static_assert(offsetof(TextPushConstants, Color) == 32);
    static_assert(offsetof(TextPushConstants, Viewport) == 48);
}
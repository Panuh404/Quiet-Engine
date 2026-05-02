#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

namespace Quiet::Renderer::Vulkan
{
    struct CameraUniformData
    {
        alignas(16) glm::mat4 View{1.0f};
        alignas(16) glm::mat4 Projection{1.0f};
    };

    struct TransformPushConstants
    {
        alignas(16) glm::mat4 Model{ 1.0f };
        alignas(16) glm::vec4 BaseColor{ 1.0f };

        // xyz = direction from surface toward light, w = intensity
        alignas(16) glm::vec4 LightDirectionIntensity{ -0.35f, 0.85f, -0.35f, 1.0f };

        // rgb = direct light color, w unused
        alignas(16) glm::vec4 LightColor{ 1.0f, 1.0f, 1.0f, 1.0f };

        // rgb = ambient color, w = ambient intensity
        alignas(16) glm::vec4 AmbientColorIntensity{ 1.0f, 1.0f, 1.0f, 0.25f };
    };
}
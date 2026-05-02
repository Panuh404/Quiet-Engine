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
        alignas(16) glm::mat4 Model{1.0f};
        alignas(16) glm::vec4 BaseColor{ 1.0f };
    };
}
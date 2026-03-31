#pragma once
#include "glm/fwd.hpp"
#include "glm/matrix.hpp"

namespace Quiet::Renderer::Vulkan
{
    struct TransformPushConstants
    {
        alignas(16) glm::mat4 MVP{1.0f};
    };
}


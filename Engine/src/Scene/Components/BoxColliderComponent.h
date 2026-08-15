#pragma once

#include <glm/vec3.hpp>

namespace Quiet::Scene
{
    struct BoxColliderComponent
    {
        glm::vec3 Size{ 1.0f, 1.0f, 1.0f };
        glm::vec3 Center{ 0.0f, 0.0f, 0.0f };
    };
}
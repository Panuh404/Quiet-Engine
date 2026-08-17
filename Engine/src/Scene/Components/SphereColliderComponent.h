#pragma once

#include <glm/vec3.hpp>

namespace Quiet::Scene
{
    struct SphereColliderComponent
    {
        float Radius = 0.5f;
        glm::vec3 Center { 0.0f, 0.0f, 0.0f };
    };
}
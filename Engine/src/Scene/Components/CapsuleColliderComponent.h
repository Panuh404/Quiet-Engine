#pragma once

#include <glm/vec3.hpp>

namespace Quiet::Scene
{
    struct CapsuleColliderComponent
    {
        float Radius = 0.5f;
        float Height = 2.0f;

        glm::vec3 Center = glm::vec3( 0.0f, 0.0f ,0.0f );
    };
}
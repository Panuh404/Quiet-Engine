#pragma once

#include "glm/vec3.hpp"

namespace Quiet::Scene
{
    struct DirectionalLightComponent
    {
        bool Enabled = true;

        // Direction from surface toward light.
        glm::vec3 Direction{ -0.35f, 0.85f, -0.35f };

        glm::vec3 Color{ 1.0f, 1.0f, 1.0f };
        float Intensity = 1.0f;
    };
}
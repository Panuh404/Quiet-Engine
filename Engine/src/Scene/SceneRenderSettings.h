#pragma once

#include "glm/vec3.hpp"

namespace Quiet::Scene
{
    struct SceneRenderSettings
    {
        // Direction from surface toward light.
        glm::vec3 LightDirection{ -0.35f, 0.85f, -0.35f };
        float LightIntensity = 1.0f;

        glm::vec3 AmbientColor{ 1.0f, 1.0f, 1.0f };
        float AmbientIntensity = 0.25f;
    };
}
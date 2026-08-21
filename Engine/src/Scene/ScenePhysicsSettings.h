#pragma once

#include <glm/vec3.hpp>

namespace Quiet::Scene
{
    struct ScenePhysicsSettings
    {
        glm::vec3 Gravity{ 0.0f, -9.81f, 0.0f };
    };
}
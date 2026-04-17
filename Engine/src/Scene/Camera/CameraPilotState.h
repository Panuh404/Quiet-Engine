#pragma once

#include "glm/vec3.hpp"

namespace Quiet::Scene
{
    struct CameraPilotState
    {
        glm::vec3 Position = glm::vec3(0.0f);
        float YawDegrees = -90.0f;
        float PitchDegrees = 0.0f;
        float MoveSpeed = 8.0f;
        float LookSensitivity = 0.12f;
    };
}

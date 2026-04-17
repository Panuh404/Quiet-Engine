#pragma once

#include "glm/vec3.hpp"

namespace Quiet::Scene
{
    struct CameraPilotState
    {
        glm::vec3 Position = glm::vec3(0.0f);
        float YawDegrees = -90.0f;
        float PitchDegrees = 0.0f;
        float MoveSpeed = 2.5f;
        float LookSensitivity = 0.08f;
    };
}

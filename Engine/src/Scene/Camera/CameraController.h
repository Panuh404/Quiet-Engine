#pragma once
#include "CameraPilotState.h"

namespace Quiet::Input
{
    class Input;
}

namespace Quiet::Scene
{
    class CameraController
    {
    public:
        static void UpdateFromInput(CameraPilotState& state, const Input::Input& input, float deltaTime);

        static void OrbitAround(CameraPilotState& state, const glm::vec3& pivot, double mouseDeltaX, double mouseDeltaY);
        static void Pan(CameraPilotState& state, glm::vec3& pivot, double mouseDeltaX, double mouseDeltaY, float viewportHeight, float fieldOfViewDegrees);
    };
}

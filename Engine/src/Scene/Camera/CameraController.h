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
        static void UpdateFromInput(CameraPilotState& state,
                                    const Input::Input& input,
                                    float deltaTime);
    };
}

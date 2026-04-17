#pragma once

#include "CameraPilotState.h"
#include "Scene/Transform.h"

namespace Quiet::Scene
{
    inline CameraPilotState BuildPilotStateFromCameraTransform(const Transform& transform)
    {
        CameraPilotState state{};
        state.Position = transform.Position;
        state.YawDegrees = -transform.RotationDegrees.y - 90.0f;
        state.PitchDegrees = transform.RotationDegrees.x;

        return state;
    }

    inline void ApplyPilotStateToCameraTransform(const CameraPilotState& state,
                                                 Transform& transform)
    {
        transform.Position = state.Position;
        transform.RotationDegrees.x = state.PitchDegrees;
        transform.RotationDegrees.y = -(state.YawDegrees + 90.0f);
    }
}
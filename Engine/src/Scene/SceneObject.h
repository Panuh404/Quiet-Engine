#pragma once

#include "Transform.h"

namespace Quiet::Scene
{
    struct SceneObject
    {
        Transform TransformData {};
        float RotationSpeedDegreesPerSecond = 0.0f;
    };
}
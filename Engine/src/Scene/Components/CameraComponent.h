#pragma once

namespace Quiet::Scene
{
    struct CameraComponent
    {
        bool Primary = true;
        float FieldOfViewDegrees = 60.f;
        float NearClip = 0.1f;
        float FarClip = 1000.f;
    };
}
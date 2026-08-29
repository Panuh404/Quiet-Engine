#pragma once

#include <string>

namespace Quiet::Scene
{
    struct AudioSourceComponent
    {
        std::string AudioAssetID;

        float Volume = 1.0f;
        float Pitch = 1.0f;

        bool PlayOnStart = false;
        bool Loop = false;
        bool Spatial = true;

        float MinDistance = 1.0f;
        float MaxDistance = 100000.0f;
        float Rolloff = 1.0f;

        float ConeInnerAngleDegrees = 360.0f;
        float ConeOuterAngleDegrees = 360.0f;
        float ConeOuterGain = 0.0f;

        float DopplerFactor = 1.0f;
    };
}
#pragma once

#include <cstdint>

namespace Quiet::Scene
{
    enum class RigidBodyMotionType : std::uint8_t
    {
        Static,
        Dynamic,
        Kinematic
    };

    struct RigidBodyComponent
    {
        RigidBodyMotionType MotionType = RigidBodyMotionType::Dynamic;

        float Friction = 0.2f;
        float Restitution = 0.0f;

        float LinearDamping = 0.05f;
        float AngularDamping = 0.05f;

        float GravityFactor = 1.0f;

        bool AllowSleeping = true;
    };
}
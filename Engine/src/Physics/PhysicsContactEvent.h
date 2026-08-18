#pragma once

#include <cstdint>

#include "Quiet/Core/UUID.h"

namespace Quiet::Physics
{
    enum class PhysicsContactEventType : std::uint8_t
    {
        Added,
        Removed,
    };

    struct PhysicsContactEvent
    {
        PhysicsContactEventType Type = PhysicsContactEventType::Added;

        Core::UUID ActorA{};
        Core::UUID ActorB{};
    };
}
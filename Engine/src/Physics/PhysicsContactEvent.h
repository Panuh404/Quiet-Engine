#pragma once

#include <cstdint>

#include "Quiet/Core/UUID.h"

namespace Quiet::Physics
{
    enum class PhysicsContactEventType : std::uint8_t
    {
        Started,
        Ended
    };

    struct PhysicsContactEvent
    {
        PhysicsContactEventType Type = PhysicsContactEventType::Started;

        Core::UUID ActorA{};
        Core::UUID ActorB{};
    };
}
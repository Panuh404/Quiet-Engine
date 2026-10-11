#pragma once

#include <cstdint>

#include "Quiet/Core/UUID.h"

namespace Quiet::Physics
{
    /**
     * @ingroup quiet_physics
     * @brief Identifies the beginning or end of a physics contact.
     */
    enum class PhysicsContactEventType : std::uint8_t
    {
        Started,
        Ended
    };

    /**
     * @ingroup quiet_physics
     * @brief Buffered contact notification identifying two Actors.
     * @note Contains no contact geometry or impulse data.
     */
    struct PhysicsContactEvent
    {
        PhysicsContactEventType Type = PhysicsContactEventType::Started;

        Core::UUID ActorA{};
        Core::UUID ActorB{};
    };
}
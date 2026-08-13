#pragma once

#include <string>

#include "Quiet/Core/UUID.h"

namespace Quiet::Scene
{
    struct ActorIdentityComponent
    {
        Core::UUID ID{};
        std::string Name;
    };
}
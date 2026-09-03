#pragma once

#include <sol/sol.hpp>

#include "Quiet/Core/UUID.h"

namespace Quiet::Scene
{
    class Scene;
}

namespace Quiet::Scripting::Bindings
{
    void InstallActorBindings(sol::state& lua, sol::environment& environment, Scene::Scene& scene, Core::UUID actorID);
}
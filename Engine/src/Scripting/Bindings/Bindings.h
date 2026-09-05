#pragma once

#include <sol/sol.hpp>

#include "Quiet/Core/UUID.h"

namespace Quiet::Input
{
    class Input;
}

namespace Quiet::Scene
{
    class Scene;
}

namespace Quiet::Scripting::Bindings
{
    void InstallActorBindings(sol::state& lua, sol::environment& environment, Scene::Scene& scene, Core::UUID actorID);

    void InstallInputBindings(sol::state& lua, sol::environment& environment, const Input::Input& input);

    void InstallPhysicsBindings(sol::state& lua, sol::environment& environment, Scene::Scene& scene, Core::UUID actorID);
}
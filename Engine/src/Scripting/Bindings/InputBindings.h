#pragma once

#include <sol/sol.hpp>

namespace Quiet::Input
{
    class Input;
}

namespace Quiet::Scripting::Bindings
{
    void InstallInputBindings(sol::state& lua, sol::environment& environment, const Input::Input& input);
}
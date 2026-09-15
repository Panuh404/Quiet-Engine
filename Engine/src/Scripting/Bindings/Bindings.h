#pragma once

#include <sol/sol.hpp>

#include "Quiet/Core/UUID.h"

namespace Quiet::Project
{
    class ProjectContext;
}

namespace Quiet::Input
{
    class Input;
}

namespace Quiet::Scene
{
    class Scene;
    struct ScriptComponent;
}

namespace Quiet::Scripting::Bindings
{
    sol::table CreateActorReference(sol::state& lua, Scene::Scene& scene, Core::UUID actorID);

    void InstallPropertyBindings(sol::state& lua, sol::environment& environment, Scene::Scene& scene, const Project::ProjectContext& projectContext, const Scene::ScriptComponent& script);
    void InstallActorBindings(sol::state& lua, sol::environment& environment, Scene::Scene& scene, Core::UUID actorID);
    void InstallSceneBindings(sol::state& lua, sol::environment& environment, Scene::Scene& scene);
    void InstallInputBindings(sol::state& lua, sol::environment& environment, const Input::Input& input);
    void InstallPhysicsBindings(sol::state& lua, sol::table& actorTable, Scene::Scene& scene, Core::UUID actorID);
    void InstallAudioBindings(sol::state& lua, sol::table& actorTable, Scene::Scene& scene, Core::UUID actorID);
}
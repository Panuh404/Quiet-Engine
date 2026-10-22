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
    /**
     * @brief Creates a Lua Actor table whose operations resolve a captured UUID.
     * @ingroup quiet_scripting
     * @note Does not own or keep the Actor alive.
     * @note May represent a pending creation that does not yet resolve.
     * @note Actor-dependent operations require the Actor to remain available.
     */
    sol::table CreateActorReference(sol::state& lua, Scene::Scene& scene, Core::UUID actorID);

    /**
     * @brief Installs Expose functions using a snapshot of authored properties.
     * @ingroup quiet_scripting
     * @note Matches property name and type; later component edits are not reflected.
     * @note Actor and asset references return nil when resolution checks fail.
     */
    void InstallPropertyBindings(sol::state& lua, sol::environment& environment, Scene::Scene& scene, const Project::ProjectContext& projectContext, const Scene::ScriptComponent& script);

    /**
     * @brief Installs the owning Actor reference in the script environment.
     * @ingroup quiet_scripting
     * @note Includes Transform, Text, UIRect, RigidBody, and Audio tables.
     * @note Operations resolve the captured UUID against the borrowed Scene.
     */
    void InstallActorBindings(sol::state& lua, sol::environment& environment, Scene::Scene& scene, Core::UUID actorID);

    /**
     * @brief Installs Scene lookup and deferred runtime Actor creation bindings.
     * @ingroup quiet_scripting
     * @note Returned creation references can precede Actor materialization.
     */
    void InstallSceneBindings(sol::state& lua, sol::environment& environment, Scene::Scene& scene);

    /**
     * @brief Installs input queries and named keyboard and mouse-button codes.
     * @ingroup quiet_scripting
     * @note Borrows Input; unsupported numeric codes return false.
     */
    void InstallInputBindings(sol::state& lua, sol::environment& environment, const Input::Input& input);

    /**
     * @brief Installs impulse and linear-velocity commands under Actor.RigidBody.
     * @ingroup quiet_scripting
     * @note Commands forward through Scene and return operation success.
     * @note Installed commands require a bound Dynamic body and finite vector input.
     */
    void InstallPhysicsBindings(sol::state& lua, sol::table& actorTable, Scene::Scene& scene, Core::UUID actorID);

    /**
     * @brief Installs playback commands and state queries under Actor.Audio.
     * @ingroup quiet_scripting
     * @note Commands control the Actor's runtime source through Scene.
     * @note GetState returns Unavailable, Stopped, Playing, or Paused as text.
     * @note Does not control subsystem one-shots or editor previews.
     */
    void InstallAudioBindings(sol::state& lua, sol::table& actorTable, Scene::Scene& scene, Core::UUID actorID);
}
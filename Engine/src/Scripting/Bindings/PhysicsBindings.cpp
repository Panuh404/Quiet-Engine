#include "Scripting/Bindings/Bindings.h"

#include <cmath>
#include <stdexcept>

#include <glm/vec3.hpp>

#include "Scene/Scene.h"

namespace Quiet::Scripting::Bindings
{
    void InstallPhysicsBindings(sol::state& lua, sol::environment& environment, Scene::Scene& scene, const Core::UUID actorID)
    {
        const sol::object actorObject = environment["Actor"];
        if (!actorObject.valid() || actorObject.get_type() != sol::type::table)
        {
            throw std::runtime_error("Actor bindings must be installed before physics bindings.");
        }

        sol::table actorTable = actorObject.as<sol::table>();
        sol::table rigidBodyTable = lua.create_table();

        rigidBodyTable.set_function("ApplyImpulse", [&scene, actorID](const float x, const float y, const float z)
        {
            if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z)) return false;
            return scene.ApplyPhysicsImpulse(actorID, glm::vec3{x, y, z});
        });

        rigidBodyTable.set_function("SetLinearVelocity", [&scene, actorID](const float x, const float y, const float z)
        {
            if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z)) return false;
            return scene.SetPhysicsLinearVelocity(actorID, glm::vec3{x, y, z});
        });

        actorTable["RigidBody"] = rigidBodyTable;
    }
}
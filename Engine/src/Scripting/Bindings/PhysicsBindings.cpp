#include "Scripting/Bindings/Bindings.h"

#include <cmath>

#include <glm/vec3.hpp>

#include "Scene/Scene.h"

namespace Quiet::Scripting::Bindings
{
    void InstallPhysicsBindings(sol::state& lua, sol::table& actorTable, Scene::Scene& scene, const Core::UUID actorID)
    {
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
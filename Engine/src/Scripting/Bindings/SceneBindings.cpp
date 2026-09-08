#include "Scripting/Bindings/Bindings.h"

#include <algorithm>
#include <string>

#include "Quiet/Core/UUID.h"
#include "Scene/Scene.h"

namespace Quiet::Scripting::Bindings
{
    void InstallSceneBindings(sol::state& lua, sol::environment& environment, Scene::Scene& scene)
    {
        sol::table sceneTable = lua.create_table();

        sceneTable.set_function("CreateActor", [&lua, &scene](const std::string& name) -> sol::object
        {
            const Core::UUID actorID = scene.RequestRuntimeActorCreate(name);
            if (!actorID.IsValid())
            {
                return sol::make_object(lua, sol::nil);
            }

            return sol::make_object(lua, CreateActorReference(lua, scene, actorID));
    });

        sceneTable.set_function("FindActorByID", [&lua, &scene](const std::string& text) -> sol::object
        {
            Core::UUID actorID{};
            if (!Core::UUIDFromString(text, actorID) || !actorID.IsValid() || !scene.FindActorByID(actorID))
            {
                return sol::make_object(lua, sol::nil);
            }

            return sol::make_object(lua, CreateActorReference(lua, scene, actorID));
        });

        sceneTable.set_function("FindFirstActorByName", [&lua, &scene](const std::string& name) -> sol::object
        {
            const auto& actors = scene.GetActors();
            const auto iterator =
                std::find_if(
                    actors.begin(),
                    actors.end(),
                    [&name](const Scene::Actor& actor)
                        {
                            return actor.GetName() == name;
                        });

            if (iterator == actors.end())
            {
                return sol::make_object(lua, sol::nil);
            }

            return sol::make_object(lua, CreateActorReference(lua, scene, iterator->GetID()));
        });

        environment["Scene"] = sceneTable;
    }
}
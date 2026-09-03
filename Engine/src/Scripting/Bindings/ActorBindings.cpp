#include "Scripting/Bindings/ActorBindings.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <tuple>

#include "Quiet/Core/UUID.h"
#include "Scene/Scene.h"

namespace Quiet::Scripting::Bindings
{
    void InstallActorBindings(sol::state& lua, sol::environment& environment, Scene::Scene& scene, const Core::UUID actorID)
    {
        const auto requireActor = [&scene, actorID]() -> Scene::Actor&
        {
            Scene::Actor* actor = scene.FindActorByID(actorID);
            if (!actor) throw std::runtime_error("The script Actor is no longer available.");
            return *actor;
        };

        sol::table actorTable = lua.create_table();

        #pragma region Actor

        actorTable.set_function("IsValid", [&scene, actorID]()
        {
            return scene.FindActorByID(actorID) != nullptr;
        });

        actorTable.set_function("GetID", [actorID]()
        {
            return Core::UUIDToString(actorID);
        });

        actorTable.set_function("GetName", [requireActor]()
        {
            return requireActor().GetName();
        });

        #pragma endregion Actor

        #pragma region Transform

        sol::table transformTable = lua.create_table();

        transformTable.set_function("GetPosition", [requireActor]()
        {
            const auto& position = requireActor().GetComponent<Scene::TransformComponent>().WorldTransform.Position;
            return std::make_tuple(position.x, position.y, position.z);
        });

        transformTable.set_function("SetPosition", [requireActor](const float x, const float y, const float z)
        {
            if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z)) return false;
            auto& transform = requireActor().GetComponent<Scene::TransformComponent>().WorldTransform;
            transform.Position = { x, y, z };
            return true;
        });

        transformTable.set_function("GetRotation", [requireActor]()
        {
            const auto& rotation = requireActor().GetComponent<Scene::TransformComponent>().WorldTransform.RotationDegrees;
            return std::make_tuple(rotation.x, rotation.y, rotation.z);
        });

        transformTable.set_function("SetRotation", [requireActor](const float x, const float y, const float z)
        {
            if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z)) return false;
            auto& transform = requireActor().GetComponent<Scene::TransformComponent>().WorldTransform;
            transform.RotationDegrees = { x, y, z };
            return true;
        });

        transformTable.set_function("GetScale", [requireActor]()
        {
            const auto& scale = requireActor().GetComponent<Scene::TransformComponent>().WorldTransform.Scale;
            return std::make_tuple(scale.x, scale.y, scale.z);
        });

        transformTable.set_function("SetScale", [requireActor](const float x, const float y, const float z)
        {
            if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z)) return false;
            auto& transform = requireActor().GetComponent<Scene::TransformComponent>().WorldTransform;
            transform.Scale =
                {
                    std::clamp(x, 0.01f, 100.0f),
                    std::clamp(y, 0.01f, 100.0f),
                    std::clamp(z, 0.01f, 100.0f)
                };
            return true;
        });

        actorTable["Transform"] = transformTable;

        #pragma endregion

        environment["Actor"] = actorTable;
    }
}
#include "Scripting/Bindings/Bindings.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <tuple>
#include <string>

#include "Quiet/Core/UUID.h"
#include "Scene/Components/TextComponent.h"
#include "Scene/Components/UIRectComponent.h"
#include "Scene/Scene.h"

namespace Quiet::Scripting::Bindings
{
    sol::table CreateActorReference(sol::state& lua, Scene::Scene& scene, const Core::UUID actorID)
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

        actorTable.set_function("Destroy", [&scene, actorID]()
        {
            return scene.RequestRuntimeActorDestroy(actorID);
        });

        actorTable.set_function("Duplicate", [&lua, &scene, actorID]() -> sol::object
        {
            const Core::UUID duplicateID = scene.RequestRuntimeActorDuplicate(actorID);
            if (!duplicateID.IsValid())
            {
                return sol::make_object(lua, sol::nil);
            }

            return sol::make_object(lua, CreateActorReference(lua, scene, duplicateID));
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

        InstallPhysicsBindings(lua, actorTable, scene, actorID);

        #pragma region Text

        sol::table textTable = lua.create_table();

        textTable.set_function("HasComponent", [requireActor]()
        {
            return requireActor().HasComponent<Scene::TextComponent>();
        });

        textTable.set_function("GetContent", [requireActor]() -> std::string
        {
            const auto* text =
                requireActor().TryGetComponent<Scene::TextComponent>();

            if (!text)
            {
                throw std::runtime_error(
                    "The script Actor has no Text component.");
            }

            return text->Content;
        });

        textTable.set_function(
            "SetContent",
            [requireActor](const std::string& content)
            {
                auto* text =
                    requireActor().TryGetComponent<Scene::TextComponent>();

                if (!text) return false;

                text->Content = content;
                return true;
            });

        textTable.set_function("SetVisible", [requireActor](const bool visible)
        {
            auto* text =
                requireActor().TryGetComponent<Scene::TextComponent>();

            if (!text) return false;

            text->Visible = visible;
            return true;
        });

        textTable.set_function("SetPixelHeight", [requireActor](const float pixelHeight)
        {
            if (!std::isfinite(pixelHeight)) return false;

            auto* text = requireActor().TryGetComponent<Scene::TextComponent>();
            if (!text) return false;

            text->PixelHeight = std::clamp(pixelHeight, 1.0f, 256.0f);
            return true;
        });

        textTable.set_function("SetColor", [requireActor](const float r, const float g, const float b, const float a)
        {
            if (!std::isfinite(r) || !std::isfinite(g) || !std::isfinite(b) || !std::isfinite(a)) return false;

            auto* text = requireActor().TryGetComponent<Scene::TextComponent>();
            if (!text) return false;

            text->Color = glm::vec4(
                std::clamp(r, 0.0f, 1.0f),
                std::clamp(g, 0.0f, 1.0f),
                std::clamp(b, 0.0f, 1.0f),
                std::clamp(a, 0.0f, 1.0f));

            return true;
        });

        actorTable["Text"] = textTable;

#pragma endregion

        #pragma region UI Rectangle

        sol::table uiRectTable = lua.create_table();

        uiRectTable.set_function("HasComponent", [requireActor]()
        {
            return requireActor().HasComponent<Scene::UIRectComponent>();
        });

        uiRectTable.set_function("GetOffset", [requireActor]()
        {
            const auto* uiRect = requireActor().TryGetComponent<Scene::UIRectComponent>();
            if (!uiRect) throw std::runtime_error("The script Actor has no UI Rectangle component.");

            return std::make_tuple(uiRect->Offset.x, uiRect->Offset.y);
        });

        uiRectTable.set_function("SetOffset", [requireActor](const float x, const float y)
        {
            if (!std::isfinite(x) || !std::isfinite(y)) return false;

            auto* uiRect = requireActor().TryGetComponent<Scene::UIRectComponent>();
            if (!uiRect) return false;

            uiRect->Offset = glm::vec2(x, y);
            return true;
        });

        actorTable["UIRect"] = uiRectTable;

        #pragma endregion

        InstallAudioBindings(lua, actorTable, scene, actorID);

        return actorTable;
    }

    void InstallActorBindings(sol::state& lua, sol::environment& environment, Scene::Scene& scene, const Core::UUID actorID)
    {
        environment["Actor"] = CreateActorReference(lua, scene, actorID);
    }
}
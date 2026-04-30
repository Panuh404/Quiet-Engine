#include "SceneSerializer.h"

#include <fstream>
#include <nlohmann/json.hpp>

#include "Quiet/Core/Log.h"
#include "Quiet/Core/UUID.h"
#include "Renderer/Vulkan/Mesh/MeshLibrary.h"
#include "Scene/Scene.h"

namespace Quiet::Scene
{
    using json = nlohmann::json;

    static json SerializeVec3(const glm::vec3& value)
    {
        return json::array({ value.x, value.y, value.z });
    }

    static glm::vec3 DeserializeVec3(const json& value)
    {
        return {
            value.at(0).get<float>(),
            value.at(1).get<float>(),
            value.at(2).get<float>()
        };
    }

    bool SceneSerializer::SaveToFile(const Scene &scene, const std::filesystem::path &path)
    {
        json root;
        root["actors"] = json::array();

        for (const auto& actor : scene.GetActors())
        {
            json actorJson;
            actorJson["id"] = Quiet::Core::UUIDToString(actor.ID);
            actorJson["name"] = actor.Name;

            actorJson["transform"] = {
                { "position", SerializeVec3(actor.Transform.WorldTransform.Position) },
                { "rotation", SerializeVec3(actor.Transform.WorldTransform.RotationDegrees) },
                { "scale", SerializeVec3(actor.Transform.WorldTransform.Scale) }
            };

            if (actor.StaticMesh.has_value() && actor.StaticMesh->IsValid())
            {
                actorJson["static_mesh"] = {
                    { "resource_id", actor.StaticMesh->MeshResourceID },
                    { "visible", actor.StaticMesh->Visible }
                };
            }

            if (actor.Rotator.has_value())
            {
                actorJson["rotator"] = {
                    { "speed_degrees_per_second", actor.Rotator->RotationSpeedDegreesPerSecond }
                };
            }

            if (actor.Camera.has_value())
            {
                actorJson["camera"] = SerializeCamera(actor.Camera.value());
            }

            root["actors"].push_back(std::move(actorJson));
        }

        std::ofstream out(path);
        if (!out.is_open())
        {
            QUIET_CORE_ERROR("Failed to open scene file for writing: {}", path.string());
            return false;
        }

        out << root.dump(4);
        QUIET_CORE_INFO("Saved scene to '{}'.", path.string());
        return true;
    }

    bool SceneSerializer::LoadFromFile(Scene &scene, const Renderer::Vulkan::MeshLibrary &meshLibrary,
        const std::filesystem::path &path)
    {
        std::ifstream in(path);
        if (!in.is_open())
        {
            QUIET_CORE_WARN("Scene file not found: {}", path.string());
            return false;
        }

        json root;
        in >> root;

        if (!root.contains("actors") || !root["actors"].is_array())
        {
            QUIET_CORE_ERROR("Invalid scene file format: {}", path.string());
            return false;
        }

        scene.Clear();
        for (const auto& actorJson : root["actors"])
        {
            Core::UUID id{};
            if (!Quiet::Core::UUIDFromString(actorJson.at("id").get<std::string>(), id))
            {
                QUIET_CORE_ERROR("Invalid actor UUID in scene file.");
                continue;
            }

            auto& actor = scene.CreateActorWithID(id, actorJson.value("name", "Actor"));

            const auto& transformJson = actorJson.at("transform");
            actor.Transform.WorldTransform.Position = DeserializeVec3(transformJson.at("position"));
            actor.Transform.WorldTransform.RotationDegrees = DeserializeVec3(transformJson.at("rotation"));
            actor.Transform.WorldTransform.Scale = DeserializeVec3(transformJson.at("scale"));

            if (actorJson.contains("static_mesh"))
            {
                const auto& staticMeshJson = actorJson["static_mesh"];
                const std::string resourceId = staticMeshJson.at("resource_id").get<std::string>();

                if (auto mesh = meshLibrary.TryGet(resourceId))
                {
                    actor.StaticMesh = StaticMeshComponent{};
                    actor.StaticMesh->MeshResourceID = resourceId;
                    actor.StaticMesh->Mesh = mesh;
                    actor.StaticMesh->Visible = staticMeshJson.value("visible", true);
                }
                else
                {
                    QUIET_CORE_WARN("Scene references missing mesh resource '{}'.", resourceId);
                }
            }

            if (actorJson.contains("rotator"))
            {
                const auto& rotatorJson = actorJson["rotator"];
                actor.Rotator = RotatorComponent{
                    rotatorJson.value("speed_degrees_per_second", 0.0f)
                };
            }

            if (actorJson.contains("camera"))
            {
                DeserializeCamera(actorJson["camera"], actor);
            }
        }

        QUIET_CORE_INFO("Loaded scene from '{}'.", path.string());
        return true;
    }

    nlohmann::json SceneSerializer::SerializeCamera(const CameraComponent& camera)
    {
        return {
            { "Primary", camera.Primary },
            { "FieldOfViewDegrees", camera.FieldOfViewDegrees },
            { "NearClip", camera.NearClip },
            { "FarClip", camera.FarClip }
        };
    }

    void SceneSerializer::DeserializeCamera(const nlohmann::json& json, Actor& actor)
    {
        CameraComponent camera{};

        if (json.contains("Primary"))
            camera.Primary = json["Primary"].get<bool>();

        if (json.contains("FieldOfViewDegrees"))
            camera.FieldOfViewDegrees = json["FieldOfViewDegrees"].get<float>();

        if (json.contains("NearClip"))
            camera.NearClip = json["NearClip"].get<float>();

        if (json.contains("FarClip"))
            camera.FarClip = json["FarClip"].get<float>();

        actor.Camera = camera;
    }
}

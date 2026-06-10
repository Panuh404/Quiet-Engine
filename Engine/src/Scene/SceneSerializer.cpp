#include "SceneSerializer.h"

#include <fstream>
#include <optional>
#include <utility>

#include <nlohmann/json.hpp>
#include "glm/vec4.hpp"

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

    static json SerializeVec4(const glm::vec4& value)
    {
        return json::array({ value.x, value.y, value.z, value.w });
    }

    static glm::vec3 DeserializeVec3(const json& value)
    {
        return {
            value.at(0).get<float>(),
            value.at(1).get<float>(),
            value.at(2).get<float>()
        };
    }

    static glm::vec4 DeserializeVec4(const json& value)
    {
        return {
            value.at(0).get<float>(),
            value.at(1).get<float>(),
            value.at(2).get<float>(),
            value.at(3).get<float>()
        };
    }

    static const char* SerializeHierarchyNodeType(const SceneHierarchyNodeType type)
    {
        switch (type)
        {
            case SceneHierarchyNodeType::Actor:
                return "actor";
            case SceneHierarchyNodeType::Folder:
                return "folder";
            default:
                return "actor";
        }
    }

    static SceneHierarchyNodeType DeserializeHierarchyNodeType(const std::string& type)
    {
        if (type == "folder") return SceneHierarchyNodeType::Folder;
        return SceneHierarchyNodeType::Actor;
    }

    static json SerializeHierarchyNode(const SceneHierarchyNode& node)
    {
        return
        {
            { "type", SerializeHierarchyNodeType(node.Type) },
            { "id", Quiet::Core::UUIDToString(node.ID) }
        };
    }

    static std::optional<SceneHierarchyNode> DeserializeHierarchyNode(const json& nodeJson)
    {
        SceneHierarchyNode node{};
        node.Type = DeserializeHierarchyNodeType(nodeJson.value("type", std::string{ "actor" }));

        const std::string idText = nodeJson.value("id", std::string{});
        if (!Core::UUIDFromString(idText, node.ID) || !node.ID.IsValid())
        {
            QUIET_CORE_WARN("Skipping hierarchy node with invalid UUID '{}'.", idText);
            return std::nullopt;
        }

        return node;
    }

    static json SerializeHierarchyFolder(const SceneHierarchyFolder& folder)
    {
        json folderJson;
        folderJson["id"] = Quiet::Core::UUIDToString(folder.ID);
        folderJson["name"] = folder.Name;
        folderJson["expanded"] = folder.Expanded;
        folderJson["children"] = json::array();

        for (const SceneHierarchyNode& child : folder.Children)
        {
            folderJson["children"].push_back(SerializeHierarchyNode(child));
        }

        return folderJson;
    }

    static std::optional<SceneHierarchyFolder> DeserializeHierarchyFolder(const json& folderJson)
    {
        SceneHierarchyFolder folder{};

        const std::string idText = folderJson.value("id", std::string{});
        if (!Core::UUIDFromString(idText, folder.ID) || !folder.ID.IsValid())
        {
            QUIET_CORE_WARN("Skipping hierarchy folder with invalid UUID '{}'.", idText);
            return std::nullopt;
        }

        folder.Name = folderJson.value("name", std::string{ "Folder" });
        folder.Expanded = folderJson.value("expanded", true);

        if (folderJson.contains("children") && folderJson["children"].is_array())
        {
            for (const json& childJson : folderJson["children"])
            {
                if (auto child = DeserializeHierarchyNode(childJson))
                {
                    folder.Children.push_back(*child);
                }
            }
        }

        return folder;
    }

    static json SerializeHierarchyState(const SceneHierarchyState& hierarchy)
    {
        json hierarchyJson;
        hierarchyJson["root"] = json::array();
        hierarchyJson["folders"] = json::array();

        for (const SceneHierarchyNode& node : hierarchy.RootNodes)
        {
            hierarchyJson["root"].push_back(SerializeHierarchyNode(node));
        }

        for (const SceneHierarchyFolder& folder : hierarchy.Folders)
        {
            hierarchyJson["folders"].push_back(SerializeHierarchyFolder(folder));
        }

        return hierarchyJson;
    }

    static SceneHierarchyState DeserializeHierarchyState(const json& hierarchyJson)
    {
        SceneHierarchyState hierarchy{};

        if (hierarchyJson.contains("root") && hierarchyJson["root"].is_array())
        {
            for (const json& nodeJson : hierarchyJson["root"])
            {
                if (auto node = DeserializeHierarchyNode(nodeJson))
                    hierarchy.RootNodes.push_back(*node);
            }
        }

        if (hierarchyJson.contains("folders") && hierarchyJson["folders"].is_array())
        {
            for (const json& folderJson : hierarchyJson["folders"])
            {
                if (auto folder = DeserializeHierarchyFolder(folderJson))
                    hierarchy.Folders.push_back(*folder);
            }
        }

        return hierarchy;
    }

    bool SceneSerializer::SaveToFile(const Scene &scene, const std::filesystem::path &path)
    {
        json root;
        root["actors"] = json::array();

        const SceneRenderSettings& renderSettings = scene.GetRenderSettings();
        root["render_settings"] = {
            { "light_direction", SerializeVec3(renderSettings.LightDirection) },
            { "light_intensity", renderSettings.LightIntensity },
            { "ambient_color", SerializeVec3(renderSettings.AmbientColor) },
            { "ambient_intensity", renderSettings.AmbientIntensity }
        };

        root["editor_hierarchy"] = SerializeHierarchyState(scene.GetHierarchy());

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
                json staticMeshJson = {
                    { "resource_id", actor.StaticMesh->MeshResourceID },
                    { "visible", actor.StaticMesh->Visible }
                };

                if (!actor.StaticMesh->MaterialSlots.empty())
                {
                    staticMeshJson["material_slots"] = json::array();
                    for (const MaterialComponent& materialSlot : actor.StaticMesh->MaterialSlots)
                    {
                        staticMeshJson["material_slots"].push_back(SerializeMaterial(materialSlot));
                    }
                }
                actorJson["static_mesh"] = std::move(staticMeshJson);
            }

            if (actor.Material.has_value())
            {
                actorJson["material"] = SerializeMaterial(actor.Material.value());
            }

            if (actor.DirectionalLight.has_value())
            {
                actorJson["directional_light"] = {
                    { "enabled", actor.DirectionalLight->Enabled },
                    { "direction", SerializeVec3(actor.DirectionalLight->Direction) },
                    { "color", SerializeVec3(actor.DirectionalLight->Color) },
                    { "intensity", actor.DirectionalLight->Intensity }
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

    bool SceneSerializer::LoadFromFile(Scene &scene, const Renderer::Vulkan::MeshLibrary &meshLibrary, const std::filesystem::path &path)
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

        if (root.contains("render_settings"))
        {
            const auto& renderSettingsJson = root["render_settings"];
            SceneRenderSettings& renderSettings = scene.GetRenderSettings();

            if (renderSettingsJson.contains("light_direction"))
            {
                renderSettings.LightDirection = DeserializeVec3(renderSettingsJson.at("light_direction"));
            }

            renderSettings.LightIntensity = renderSettingsJson.value("light_intensity", renderSettings.LightIntensity);

            if (renderSettingsJson.contains("ambient_color"))
            {
                renderSettings.AmbientColor = DeserializeVec3(renderSettingsJson.at("ambient_color"));
            }

            renderSettings.AmbientIntensity = renderSettingsJson.value("ambient_intensity", renderSettings.AmbientIntensity);
        }

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

                    if (staticMeshJson.contains("material_slots") && staticMeshJson["material_slots"].is_array())
                    {
                        actor.StaticMesh->MaterialSlots.clear();
                        for (const auto& materialSlotJson : staticMeshJson["material_slots"])
                        {
                            actor.StaticMesh->MaterialSlots.push_back(DeserializeMaterial(materialSlotJson));
                        }
                    }
                }
                else
                {
                    QUIET_CORE_WARN("Scene references missing mesh resource '{}'.", resourceId);
                }
            }

            if (actorJson.contains("material"))
            {
                const auto& materialJson = actorJson["material"];
                actor.Material = DeserializeMaterial(materialJson);
            }

            if (actorJson.contains("directional_light"))
            {
                const auto& lightJson = actorJson["directional_light"];

                DirectionalLightComponent light{};
                light.Enabled = lightJson.value("enabled", true);

                if (lightJson.contains("direction"))
                {
                    light.Direction = DeserializeVec3(lightJson.at("direction"));
                }

                if (lightJson.contains("color"))
                {
                    light.Color = DeserializeVec3(lightJson.at("color"));
                }

                light.Intensity = lightJson.value("intensity", light.Intensity);
                actor.DirectionalLight = light;
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

        if (root.contains("editor_hierarchy") && root["editor_hierarchy"].is_object())
        {
            SceneHierarchyState hierarchy =
                DeserializeHierarchyState(root["editor_hierarchy"]);

            if (hierarchy.RootNodes.empty() && hierarchy.Folders.empty() && !scene.GetActors().empty())
            {
                scene.ResetHierarchyFromActors();
            }
            else
            {
                scene.SetHierarchy(std::move(hierarchy));
            }
        }
        else
        {
            scene.ResetHierarchyFromActors();
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

    nlohmann::json SceneSerializer::SerializeMaterial(const MaterialComponent &material)
    {
        return {
            { "base_color", SerializeVec4(material.BaseColor) },
            { "base_color_texture_asset_id", material.BaseColorTextureAssetID }
        };
    }

    MaterialComponent SceneSerializer::DeserializeMaterial(const nlohmann::json &materialJson)
    {
        MaterialComponent material{};

        if (materialJson.contains("base_color"))
            material.BaseColor = DeserializeVec4(materialJson.at("base_color"));

        material.BaseColorTextureAssetID = materialJson.value("base_color_texture_asset_id", std::string{});
        return material;
    }
}

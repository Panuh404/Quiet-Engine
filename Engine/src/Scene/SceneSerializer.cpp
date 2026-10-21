#include "SceneSerializer.h"

#include <fstream>
#include <optional>
#include <utility>

#include <nlohmann/json.hpp>
#include "glm/vec4.hpp"

#include "Quiet/Core/Log.h"
#include "Renderer/Vulkan/Mesh/MeshLibrary.h"
#include "Scene/Scene.h"

namespace Quiet::Scene
{
    using json = nlohmann::json;

#pragma region Serialization

    static json SerializeVec2(const glm::vec2& value)
    {
        return json::array({ value.x, value.y });
    }

    static json SerializeVec3(const glm::vec3& value)
    {
        return json::array({ value.x, value.y, value.z });
    }

    static json SerializeVec4(const glm::vec4& value)
    {
        return json::array({ value.x, value.y, value.z, value.w });
    }

    static const char* SerializeHierarchyNodeType(const SceneHierarchyNodeType type)
    {
        switch (type)
        {
            case SceneHierarchyNodeType::Actor:     return "actor";
            case SceneHierarchyNodeType::Folder:    return "folder";
            default:                                return "actor";
        }
    }

    static json SerializeHierarchyNode(const SceneHierarchyNode& node)
    {
        return
        {
                { "type", SerializeHierarchyNodeType(node.Type) },
                { "id", Quiet::Core::UUIDToString(node.ID) }
        };
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

    nlohmann::json SceneSerializer::SerializeCamera(const CameraComponent& camera)
    {
        return {
                { "Primary", camera.Primary },
                { "FieldOfViewDegrees", camera.FieldOfViewDegrees },
                { "NearClip", camera.NearClip },
                { "FarClip", camera.FarClip }
        };
    }

    nlohmann::json SceneSerializer::SerializeMaterial(const MaterialComponent &material)
    {
        return {
                { "base_color", SerializeVec4(material.BaseColor) },
                { "base_color_texture_asset_id", material.BaseColorTextureAssetID }
        };
    }

    static const char* SerializeAudioBus(const Audio::AudioBus bus)
    {
        switch (bus)
        {
            case Audio::AudioBus::Master:   return "master";
            case Audio::AudioBus::Music:    return "music";
            case Audio::AudioBus::SFX:      return "sfx";
            case Audio::AudioBus::UI:       return "ui";
            default:                        return "sfx";
        }
    }

    static const char* SerializeRigidBodyMotionType(const RigidBodyMotionType type)
    {
        switch (type)
        {
            case RigidBodyMotionType::Static:       return "static";
            case RigidBodyMotionType::Kinematic:    return "kinematic";
            case RigidBodyMotionType::Dynamic:
            default:                                return "dynamic";
        }
    }

    static const char* SerializeScriptPropertyType(const ScriptPropertyType type)
    {
        switch (type)
        {
            case ScriptPropertyType::Float:        return "float";
            case ScriptPropertyType::Int:          return "int";
            case ScriptPropertyType::Bool:         return "bool";
            case ScriptPropertyType::String:       return "string";

            case ScriptPropertyType::Vector2:      return "vector2";
            case ScriptPropertyType::Vector3:      return "vector3";
            case ScriptPropertyType::Vector4:      return "vector4";

            case ScriptPropertyType::Color3:       return "color3";
            case ScriptPropertyType::Color4:       return "color4";

            case ScriptPropertyType::Actor:        return "actor";

            case ScriptPropertyType::MeshAsset:    return "mesh_asset";
            case ScriptPropertyType::TextureAsset: return "texture_asset";
            case ScriptPropertyType::AudioAsset:   return "audio_asset";
            case ScriptPropertyType::ScriptAsset:  return "script_asset";

            default:
                return "float";
        }
    }

#pragma endregion

#pragma region Deserialization

    static glm::vec2 DeserializeVec2(const json& value)
    {
        return {
            value.at(0).get<float>(),
            value.at(1).get<float>()
        };
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

    static SceneHierarchyNodeType DeserializeHierarchyNodeType(const std::string& type)
    {
        if (type == "folder") return SceneHierarchyNodeType::Folder;
        return SceneHierarchyNodeType::Actor;
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

        actor.AddComponent<CameraComponent>(camera);
    }

    MaterialComponent SceneSerializer::DeserializeMaterial(const nlohmann::json &materialJson)
    {
        MaterialComponent material{};

        if (materialJson.contains("base_color"))
            material.BaseColor = DeserializeVec4(materialJson.at("base_color"));

        material.BaseColorTextureAssetID = materialJson.value("base_color_texture_asset_id", std::string{});
        return material;
    }

    static Audio::AudioBus DeserializeAudioBus(const std::string& bus)
    {
        if (bus == "master")    return Audio::AudioBus::Master;
        if (bus == "music")     return Audio::AudioBus::Music;
        if (bus == "ui")        return Audio::AudioBus::UI;
        if (bus != "sfx")
        {
            QUIET_CORE_WARN("Unknown audio bus '{}'. Using SFX.", bus);
        }

        return Audio::AudioBus::SFX;
    }

    static RigidBodyMotionType DeserializeRigidBodyMotionType(const std::string& type)
    {
        if (type == "static")
            return RigidBodyMotionType::Static;

        if (type == "kinematic")
            return RigidBodyMotionType::Kinematic;

        if (type != "dynamic")
            QUIET_CORE_WARN("Unknown rigid body motion type '{}'. Using dynamic.", type);

        return RigidBodyMotionType::Dynamic;
    }

    static bool DeserializeScriptPropertyType(const std::string& text, ScriptPropertyType& outType)
    {
        if (text == "float")                outType = ScriptPropertyType::Float;
        else if (text == "int")             outType = ScriptPropertyType::Int;
        else if (text == "bool")            outType = ScriptPropertyType::Bool;
        else if (text == "string")          outType = ScriptPropertyType::String;
        else if (text == "vector2")         outType = ScriptPropertyType::Vector2;
        else if (text == "vector3")         outType = ScriptPropertyType::Vector3;
        else if (text == "vector4")         outType = ScriptPropertyType::Vector4;
        else if (text == "color3")          outType = ScriptPropertyType::Color3;
        else if (text == "color4")          outType = ScriptPropertyType::Color4;
        else if (text == "actor")           outType = ScriptPropertyType::Actor;
        else if (text == "mesh_asset")      outType = ScriptPropertyType::MeshAsset;
        else if (text == "texture_asset")   outType = ScriptPropertyType::TextureAsset;
        else if (text == "audio_asset")     outType = ScriptPropertyType::AudioAsset;
        else if (text == "script_asset")    outType = ScriptPropertyType::ScriptAsset;
        else                                return false;
        return true;
    }

#pragma endregion

    bool SceneSerializer::SaveToFile(const Scene &scene, const std::filesystem::path &path)
    {
        json root;
        root["actors"] = json::array();

        const SceneAudioSettings& audioSettings = scene.GetAudioSettings();
        root["audio_settings"] = {
            { "master_volume", audioSettings.MasterVolume },
            { "music_volume", audioSettings.MusicVolume },
            { "sfx_volume", audioSettings.SFXVolume },
            { "ui_volume", audioSettings.UIVolume }
        };

        const SceneRenderSettings& renderSettings = scene.GetRenderSettings();
        root["render_settings"] = {
            { "light_direction", SerializeVec3(renderSettings.LightDirection) },
            { "light_intensity", renderSettings.LightIntensity },
            { "ambient_color", SerializeVec3(renderSettings.AmbientColor) },
            { "ambient_intensity", renderSettings.AmbientIntensity }
        };
        const ScenePhysicsSettings& physicsSettings = scene.GetPhysicsSettings();
        root["physics_settings"] = {
            { "gravity", SerializeVec3(physicsSettings.Gravity) }
        };
        root["editor_hierarchy"] = SerializeHierarchyState(scene.GetHierarchy());

        for (const Actor& actor : scene.GetActors())
        {
            json actorJson;
            actorJson["id"] = Quiet::Core::UUIDToString(actor.GetID());
            actorJson["name"] = actor.GetName();

            const auto& transform = actor.GetComponent<TransformComponent>();
            actorJson["transform"] =
            {
                { "position", SerializeVec3(transform.WorldTransform.Position) },
                { "rotation", SerializeVec3(transform.WorldTransform.RotationDegrees) },
                { "scale", SerializeVec3(transform.WorldTransform.Scale) }
            };

            if (const auto* text = actor.TryGetComponent<TextComponent>())
            {
                actorJson["text"] =
                {
                    { "content", text->Content },
                    { "pixel_height", text->PixelHeight },
                    { "color", SerializeVec4(text->Color) },
                    { "visible", text->Visible }
                };
            }

            if (const auto* uiRect = actor.TryGetComponent<UIRectComponent>())
            {
                actorJson["ui_rect"] =
                {
                    { "offset", SerializeVec2(uiRect->Offset) },
                    { "anchor", SerializeVec2(uiRect->Anchor) },
                    { "pivot", SerializeVec2(uiRect->Pivot) }
                };
            }

            if (const auto* staticMesh = actor.TryGetComponent<StaticMeshComponent>(); staticMesh && staticMesh->IsValid())
            {
                json staticMeshJson =
                {
                    { "resource_id", staticMesh->MeshResourceID },
                    { "visible", staticMesh->Visible }
                };

                if (!staticMesh->MaterialSlots.empty())
                {
                    staticMeshJson["material_slots"] = json::array();
                    for (const MaterialComponent& materialSlot : staticMesh->MaterialSlots)
                    {
                        staticMeshJson["material_slots"].push_back(SerializeMaterial(materialSlot));
                    }
                }

                actorJson["static_mesh"] = std::move(staticMeshJson);
            }

            if (const auto* material = actor.TryGetComponent<MaterialComponent>())
            {
                actorJson["material"] = SerializeMaterial(*material);
            }

            if (const auto* light = actor.TryGetComponent<DirectionalLightComponent>())
            {
                actorJson["directional_light"] =
                {
                    { "enabled", light->Enabled },
                    { "direction", SerializeVec3(light->Direction) },
                    { "color", SerializeVec3(light->Color) },
                    { "intensity", light->Intensity }
                };
            }

            if (const auto* rotator = actor.TryGetComponent<RotatorComponent>())
            {
                actorJson["rotator"] =
                {
                    { "speed_degrees_per_second", rotator->RotationSpeedDegreesPerSecond }
                };
            }

            if (const auto* camera = actor.TryGetComponent<CameraComponent>())
            {
                actorJson["camera"] = SerializeCamera(*camera);
            }

            if (const auto* audioSource = actor.TryGetComponent<AudioSourceComponent>())
            {
                actorJson["audio_source"] =
                {
                    { "audio_asset_id", audioSource->AudioAssetID },
                    { "bus", SerializeAudioBus(audioSource->Bus) },
                    { "volume", audioSource->Volume },
                    { "pitch", audioSource->Pitch },
                    { "play_on_start", audioSource->PlayOnStart },
                    { "loop", audioSource->Loop },
                    { "spatial", audioSource->Spatial },
                    { "min_distance", audioSource->MinDistance },
                    { "max_distance", audioSource->MaxDistance },
                    { "rolloff", audioSource->Rolloff },
                    { "cone_inner_angle_degrees", audioSource->ConeInnerAngleDegrees },
                    { "cone_outer_angle_degrees", audioSource->ConeOuterAngleDegrees },
                    { "cone_outer_gain", audioSource->ConeOuterGain },
                    { "doppler_factor", audioSource->DopplerFactor }
                };
            }

            if (const auto* audioListener = actor.TryGetComponent<AudioListenerComponent>())
            {
                actorJson["audio_listener"] =
                {
                    { "primary", audioListener->Primary }
                };
            }

            if (const auto* script = actor.TryGetComponent<ScriptComponent>())
            {
                json scriptJson =
                {
                    { "script_asset_id", script->ScriptAssetID }
                };

                if (!script->Properties.empty())
                {
                    scriptJson["properties"] = json::array();

                    for (const ScriptProperty& property : script->Properties)
                    {
                        if (property.Name.empty()) continue;

                        json propertyJson =
                        {
                            { "name", property.Name },
                            { "type", SerializeScriptPropertyType(property.Type) }
                        };

                        switch (property.Type)
                        {
                            case ScriptPropertyType::Float:
                                propertyJson["value"] = property.FloatValue;
                                break;

                            case ScriptPropertyType::Int:
                                propertyJson["value"] = property.IntValue;
                                break;

                            case ScriptPropertyType::Bool:
                                propertyJson["value"] = property.BoolValue;
                                break;

                            case ScriptPropertyType::String:
                                propertyJson["value"] = property.StringValue;
                                break;

                            case ScriptPropertyType::Vector2:
                                propertyJson["value"] = SerializeVec2(property.Vector2Value);
                                break;

                            case ScriptPropertyType::Vector3:
                                propertyJson["value"] = SerializeVec3(property.Vector3Value);
                                break;

                            case ScriptPropertyType::Vector4:
                                propertyJson["value"] = SerializeVec4(property.Vector4Value);
                                break;

                            case ScriptPropertyType::Color3:
                                propertyJson["value"] = SerializeVec3(property.Color3Value);
                                break;

                            case ScriptPropertyType::Color4:
                                propertyJson["value"] = SerializeVec4(property.Color4Value);
                                break;

                            case ScriptPropertyType::Actor:
                                propertyJson["value"] = property.ActorValue.IsValid() ? Core::UUIDToString(property.ActorValue) : std::string{};
                                break;

                            case ScriptPropertyType::MeshAsset:
                            case ScriptPropertyType::TextureAsset:
                            case ScriptPropertyType::AudioAsset:
                            case ScriptPropertyType::ScriptAsset:
                                propertyJson["value"] = property.AssetValue;
                                break;
                        }

                        scriptJson["properties"].push_back(std::move(propertyJson));
                    }
                }

                actorJson["script"] = std::move(scriptJson);
            }

            if (const auto* rigidBody = actor.TryGetComponent<RigidBodyComponent>())
            {
                actorJson["rigid_body"] =
                {
                    { "motion_type", SerializeRigidBodyMotionType(rigidBody->MotionType) },
                    { "friction", rigidBody->Friction },
                    { "restitution", rigidBody->Restitution },
                    { "linear_damping", rigidBody->LinearDamping },
                    { "angular_damping", rigidBody->AngularDamping },
                    { "gravity_factor", rigidBody->GravityFactor },
                    { "allow_sleeping", rigidBody->AllowSleeping }
                };
            }

            if (const auto* boxCollider = actor.TryGetComponent<BoxColliderComponent>())
            {
                actorJson["box_collider"] =
                {
                    { "size", SerializeVec3(boxCollider->Size) },
                    { "center", SerializeVec3(boxCollider->Center) }
                };
            }

            if (const auto* sphereCollider = actor.TryGetComponent<SphereColliderComponent>())
            {
                actorJson["sphere_collider"] =
                {
                    { "radius", sphereCollider->Radius },
                    { "center", SerializeVec3(sphereCollider->Center) }
                };
            }

            if (const auto* capsuleCollider = actor.TryGetComponent<CapsuleColliderComponent>())
            {
                actorJson["capsule_collider"] =
                {
                    { "radius", capsuleCollider->Radius },
                    { "height", capsuleCollider->Height },
                    { "center", SerializeVec3(capsuleCollider->Center) }
                };
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

        if (root.contains("audio_settings"))
        {
            const auto& audioSettingsJson = root["audio_settings"];

            SceneAudioSettings& audioSettings = scene.GetAudioSettings();
            audioSettings.MasterVolume = audioSettingsJson.value("master_volume", audioSettings.MasterVolume);
            audioSettings.MusicVolume = audioSettingsJson.value("music_volume", audioSettings.MusicVolume);
            audioSettings.SFXVolume = audioSettingsJson.value("sfx_volume", audioSettings.SFXVolume);
            audioSettings.UIVolume = audioSettingsJson.value("ui_volume", audioSettings.UIVolume);
        }

        if (root.contains("render_settings"))
        {
            const auto& renderSettingsJson = root["render_settings"];
            SceneRenderSettings& renderSettings = scene.GetRenderSettings();

            if (renderSettingsJson.contains("light_direction"))
                renderSettings.LightDirection = DeserializeVec3(renderSettingsJson.at("light_direction"));

            renderSettings.LightIntensity = renderSettingsJson.value("light_intensity", renderSettings.LightIntensity);

            if (renderSettingsJson.contains("ambient_color"))
                renderSettings.AmbientColor = DeserializeVec3(renderSettingsJson.at("ambient_color"));

            renderSettings.AmbientIntensity = renderSettingsJson.value("ambient_intensity", renderSettings.AmbientIntensity);
        }

        if (root.contains("physics_settings"))
        {
            const auto& physicsSettingsJson = root["physics_settings"];
            ScenePhysicsSettings& physicsSettings = scene.GetPhysicsSettings();

            if (physicsSettingsJson.contains("gravity"))
                physicsSettings.Gravity = DeserializeVec3(physicsSettingsJson.at("gravity"));
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
            auto& transform = actor.GetComponent<TransformComponent>();
            transform.WorldTransform.Position = DeserializeVec3(transformJson.at("position"));
            transform.WorldTransform.RotationDegrees = DeserializeVec3(transformJson.at("rotation"));
            transform.WorldTransform.Scale = DeserializeVec3(transformJson.at("scale"));

            if (actorJson.contains("ui_rect"))
            {
                const auto& uiRectJson = actorJson.at("ui_rect");

                UIRectComponent uiRect{};

                if (uiRectJson.contains("offset"))
                    uiRect.Offset = DeserializeVec2(uiRectJson.at("offset"));

                if (uiRectJson.contains("anchor"))
                    uiRect.Anchor = DeserializeVec2(uiRectJson.at("anchor"));

                if (uiRectJson.contains("pivot"))
                    uiRect.Pivot = DeserializeVec2(uiRectJson.at("pivot"));

                actor.AddComponent<UIRectComponent>(uiRect);
            }

            if (actorJson.contains("text"))
            {
                const auto& textJson = actorJson.at("text");

                TextComponent text{};
                text.Content = textJson.value("content", text.Content);
                text.PixelHeight = textJson.value("pixel_height", text.PixelHeight);
                text.Visible = textJson.value("visible", text.Visible);

                if (textJson.contains("color"))
                    text.Color = DeserializeVec4(textJson.at("color"));

                actor.AddComponent<TextComponent>(text);
            }

            if (actorJson.contains("static_mesh"))
            {
                const auto& staticMeshJson = actorJson["static_mesh"];
                const std::string resourceId = staticMeshJson.at("resource_id").get<std::string>();

                if (auto mesh = meshLibrary.TryGet(resourceId))
                {
                    auto& staticMesh = actor.AddComponent<StaticMeshComponent>();
                    staticMesh.MeshResourceID = resourceId;
                    staticMesh.Mesh = mesh;
                    staticMesh.Visible = staticMeshJson.value("visible", true);

                    if (staticMeshJson.contains("material_slots") && staticMeshJson["material_slots"].is_array())
                    {
                        staticMesh.MaterialSlots.clear();
                        for (const auto& materialSlotJson : staticMeshJson["material_slots"])
                        {
                            staticMesh.MaterialSlots.push_back(DeserializeMaterial(materialSlotJson));
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
                actor.AddComponent<MaterialComponent>(DeserializeMaterial(materialJson));
            }

            if (actorJson.contains("directional_light"))
            {
                const auto& lightJson = actorJson["directional_light"];

                DirectionalLightComponent light{};
                light.Enabled = lightJson.value("enabled", true);

                if (lightJson.contains("direction"))
                    light.Direction = DeserializeVec3(lightJson.at("direction"));

                if (lightJson.contains("color"))
                    light.Color = DeserializeVec3(lightJson.at("color"));

                light.Intensity = lightJson.value("intensity", light.Intensity);
                actor.AddComponent<DirectionalLightComponent>(light);
            }

            if (actorJson.contains("rotator"))
            {
                const auto& rotatorJson = actorJson["rotator"];
                actor.AddComponent<RotatorComponent>(RotatorComponent{ rotatorJson.value("speed_degrees_per_second",0.0f) });
            }

            if (actorJson.contains("camera"))
            {
                DeserializeCamera(actorJson["camera"], actor);
            }

            if (actorJson.contains("audio_source"))
            {
                const auto& audioSourceJson = actorJson["audio_source"];
                AudioSourceComponent audioSource{};

                audioSource.AudioAssetID = audioSourceJson.value("audio_asset_id", std::string{});
                audioSource.Bus = DeserializeAudioBus(audioSourceJson.value("bus", std::string{ "sfx" }));
                audioSource.Volume = audioSourceJson.value("volume", audioSource.Volume);
                audioSource.Pitch = audioSourceJson.value("pitch", audioSource.Pitch);
                audioSource.PlayOnStart = audioSourceJson.value("play_on_start", audioSource.PlayOnStart);
                audioSource.Loop = audioSourceJson.value("loop", audioSource.Loop);
                audioSource.Spatial = audioSourceJson.value("spatial", audioSource.Spatial);
                audioSource.MinDistance = audioSourceJson.value("min_distance", audioSource.MinDistance);
                audioSource.MaxDistance = audioSourceJson.value("max_distance", audioSource.MaxDistance);
                audioSource.Rolloff = audioSourceJson.value("rolloff", audioSource.Rolloff);
                audioSource.ConeInnerAngleDegrees = audioSourceJson.value("cone_inner_angle_degrees", audioSource.ConeInnerAngleDegrees);
                audioSource.ConeOuterAngleDegrees = audioSourceJson.value("cone_outer_angle_degrees", audioSource.ConeOuterAngleDegrees);
                audioSource.ConeOuterGain = audioSourceJson.value("cone_outer_gain", audioSource.ConeOuterGain);
                audioSource.DopplerFactor = audioSourceJson.value("doppler_factor", audioSource.DopplerFactor);

                actor.AddComponent<AudioSourceComponent>(audioSource);
            }

            if (actorJson.contains("audio_listener"))
            {
                const auto& audioListenerJson = actorJson["audio_listener"];
                AudioListenerComponent audioListener{};

                audioListener.Primary = audioListenerJson.value("primary", audioListener.Primary);

                actor.AddComponent<AudioListenerComponent>(audioListener);
            }

            if (actorJson.contains("script"))
            {
                const auto& scriptJson = actorJson["script"];

                ScriptComponent script{};
                script.ScriptAssetID = scriptJson.value("script_asset_id", std::string{});

                if (scriptJson.contains("properties") && scriptJson["properties"].is_array())
                {
                    for (const auto& propertyJson : scriptJson["properties"])
                    {
                        if (!propertyJson.is_object()) continue;

                        ScriptProperty property{};
                        property.Name = propertyJson.value("name", std::string{});
                        if (property.Name.empty()) continue;
                        if (!DeserializeScriptPropertyType(propertyJson.value("type", std::string{}), property.Type)) continue;

                        switch (property.Type)
                        {
                            case ScriptPropertyType::Float:
                                property.FloatValue = propertyJson.value("value", 0.0f);
                                break;

                            case ScriptPropertyType::Int:
                                property.IntValue = propertyJson.value("value", std::int64_t{0});
                                break;

                            case ScriptPropertyType::Bool:
                                property.BoolValue = propertyJson.value("value", false);
                                break;

                            case ScriptPropertyType::String:
                                property.StringValue = propertyJson.value("value", std::string{});
                                break;

                            case ScriptPropertyType::Vector2:
                                if (propertyJson.contains("value"))
                                    property.Vector2Value = DeserializeVec2(propertyJson["value"]);
                                break;

                            case ScriptPropertyType::Vector3:
                                if (propertyJson.contains("value"))
                                    property.Vector3Value = DeserializeVec3(propertyJson["value"]);
                                break;

                            case ScriptPropertyType::Vector4:
                                if (propertyJson.contains("value"))
                                    property.Vector4Value = DeserializeVec4(propertyJson["value"]);
                                break;

                            case ScriptPropertyType::Color3:
                                if (propertyJson.contains("value"))
                                    property.Color3Value = DeserializeVec3(propertyJson["value"]);
                                break;

                            case ScriptPropertyType::Color4:
                                if (propertyJson.contains("value"))
                                    property.Color4Value = DeserializeVec4(propertyJson["value"]);
                                break;

                            case ScriptPropertyType::Actor:
                            {
                                const std::string value = propertyJson.value("value", std::string{});
                                if (!value.empty())
                                    Core::UUIDFromString(value, property.ActorValue);
                                break;
                            }

                            case ScriptPropertyType::MeshAsset:
                            case ScriptPropertyType::TextureAsset:
                            case ScriptPropertyType::AudioAsset:
                            case ScriptPropertyType::ScriptAsset:
                                property.AssetValue = propertyJson.value("value", std::string{});
                                break;
                        }

                        script.Properties.push_back(std::move(property));
                    }
                }

                actor.AddComponent<ScriptComponent>(script);
            }

            if (actorJson.contains("rigid_body"))
            {
                const auto& rigidBodyJson = actorJson["rigid_body"];
                RigidBodyComponent rigidBody{};

                rigidBody.MotionType = DeserializeRigidBodyMotionType(rigidBodyJson.value("motion_type", std::string{ "dynamic" }));
                rigidBody.Friction = rigidBodyJson.value("friction", rigidBody.Friction);
                rigidBody.Restitution = rigidBodyJson.value("restitution", rigidBody.Restitution);
                rigidBody.LinearDamping = rigidBodyJson.value("linear_damping", rigidBody.LinearDamping);
                rigidBody.AngularDamping = rigidBodyJson.value("angular_damping", rigidBody.AngularDamping);
                rigidBody.GravityFactor = rigidBodyJson.value("gravity_factor", rigidBody.GravityFactor);
                rigidBody.AllowSleeping = rigidBodyJson.value("allow_sleeping", rigidBody.AllowSleeping);
                actor.AddComponent<RigidBodyComponent>(rigidBody);
            }

            if (actorJson.contains("box_collider"))
            {
                const auto& boxColliderJson = actorJson["box_collider"];
                BoxColliderComponent boxCollider{};

                if (boxColliderJson.contains("size"))
                {
                    boxCollider.Size = DeserializeVec3(boxColliderJson.at("size"));
                }

                if (boxColliderJson.contains("center"))
                {
                    boxCollider.Center = DeserializeVec3(boxColliderJson.at("center"));
                }

                actor.AddComponent<BoxColliderComponent>(boxCollider);
            }

            if (actorJson.contains("sphere_collider"))
            {
                const auto& sphereColliderJson = actorJson["sphere_collider"];

                SphereColliderComponent sphereCollider{};
                sphereCollider.Radius = sphereColliderJson.value("radius", sphereCollider.Radius);

                if (sphereColliderJson.contains("center"))
                {
                    sphereCollider.Center = DeserializeVec3(sphereColliderJson.at("center"));
                }

                actor.AddComponent<SphereColliderComponent>(sphereCollider);
            }

            if (actorJson.contains("capsule_collider"))
            {
                const auto& colliderJson = actorJson["capsule_collider"];

                CapsuleColliderComponent collider{};
                collider.Radius = colliderJson.value("radius", collider.Radius);
                collider.Height = colliderJson.value("height", collider.Height);

                if (colliderJson.contains("center"))
                {
                    collider.Center = DeserializeVec3(colliderJson.at("center"));
                }

                actor.AddComponent<CapsuleColliderComponent>(collider);
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

}

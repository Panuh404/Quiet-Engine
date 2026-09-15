#include "Scripting/Bindings/Bindings.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <string>
#include <vector>

#include "Quiet/Project/AssetRegistry.h"
#include "Quiet/Project/AssetType.h"
#include "Quiet/Project/ProjectContext.h"
#include "Scene/Components/ScriptComponent.h"
#include "Scene/Scene.h"

namespace Quiet::Scripting::Bindings
{
    void InstallPropertyBindings(sol::state& lua, sol::environment& environment, Scene::Scene& scene,  const Project::ProjectContext& projectContext, const Scene::ScriptComponent& script)
    {
        const auto properties = std::make_shared<const std::vector<Scene::ScriptProperty>>(script.Properties);
        const auto findProperty = [properties](const std::string& name, const Scene::ScriptPropertyType type) -> const Scene::ScriptProperty*
        {
            const auto iterator = std::find_if(
                properties->begin(),
                properties->end(),
                [&](const Scene::ScriptProperty& property)
                {
                    return property.Name == name && property.Type == type;
                });

            return iterator != properties->end() ? &*iterator : nullptr;
        };

        const auto makeVector2 = [&lua](const glm::vec2& value)
        {
            sol::table result = lua.create_table();
            result["x"] = value.x;
            result["y"] = value.y;

            return result;
        };

        const auto makeVector3 = [&lua](const glm::vec3& value)
        {
            sol::table result = lua.create_table();
            result["x"] = value.x;
            result["y"] = value.y;
            result["z"] = value.z;
            return result;
        };

        const auto makeVector4 = [&lua](const glm::vec4& value)
        {
            sol::table result = lua.create_table();
            result["x"] = value.x;
            result["y"] = value.y;
            result["z"] = value.z;
            result["w"] = value.w;
            return result;
        };

        const auto makeColor3 = [&lua](const glm::vec3& value)
        {
            sol::table result = lua.create_table();
            result["r"] = value.r;
            result["g"] = value.g;
            result["b"] = value.b;
            return result;
        };

        const auto makeColor4 = [&lua](const glm::vec4& value)
        {
            sol::table result = lua.create_table();
            result["r"] = value.r;
            result["g"] = value.g;
            result["b"] = value.b;
            result["a"] = value.a;
            return result;
        };

        sol::table exposeTable = lua.create_table();

        exposeTable.set_function("Float", [findProperty](const std::string& name, const float defaultValue)
        {
            const auto* property = findProperty(name, Scene::ScriptPropertyType::Float);
            if (!property || !std::isfinite(property->FloatValue))
            {
                return std::isfinite(defaultValue) ? defaultValue : 0.0f;
            }
            return property->FloatValue;
        });

        exposeTable.set_function("Int", [findProperty](const std::string& name, const std::int64_t defaultValue)
        {
            const auto* property = findProperty(name, Scene::ScriptPropertyType::Int);
            return property ? property->IntValue : defaultValue;
        });

        exposeTable.set_function("Bool", [findProperty](const std::string& name, const bool defaultValue)
        {
            const auto* property = findProperty(name, Scene::ScriptPropertyType::Bool);
            return property ? property->BoolValue : defaultValue;
        });

        exposeTable.set_function("String", [findProperty](const std::string& name, const std::string& defaultValue)
        {
            const auto* property = findProperty(name, Scene::ScriptPropertyType::String);
            return property ? property->StringValue : defaultValue;
        });

        exposeTable.set_function("Vector2", [findProperty, makeVector2](const std::string& name, const float x, const float y)
        {
            const auto* property = findProperty(name, Scene::ScriptPropertyType::Vector2);
            return makeVector2(property ? property->Vector2Value : glm::vec2{ x, y });
        });

        exposeTable.set_function("Vector3", [findProperty, makeVector3](const std::string& name, const float x, const float y, const float z)
        {
            const auto* property = findProperty(name, Scene::ScriptPropertyType::Vector3);
            return makeVector3(property ? property->Vector3Value : glm::vec3{ x, y, z });
        });

        exposeTable.set_function("Vector4", [findProperty, makeVector4](const std::string& name, const float x, const float y, const float z, const float w)
        {
            const auto* property = findProperty(name, Scene::ScriptPropertyType::Vector4);
            return makeVector4(property ? property->Vector4Value : glm::vec4{ x, y, z, w });
        });

        exposeTable.set_function("Color3", [findProperty, makeColor3](const std::string& name, const float r, const float g, const float b)
        {
            const auto* property = findProperty(name, Scene::ScriptPropertyType::Color3);
            return makeColor3(property ? property->Color3Value : glm::vec3{ r, g, b });
        });

        exposeTable.set_function("Color4", [findProperty, makeColor4](const std::string& name, const float r, const float g, const float b, const float a)
        {
            const auto* property = findProperty(name, Scene::ScriptPropertyType::Color4);
            return makeColor4(property ? property->Color4Value : glm::vec4{r, g, b, a});
        });

        exposeTable.set_function("Actor", [&lua, &scene, findProperty](const std::string& name) -> sol::object
        {
            const auto* property = findProperty(name, Scene::ScriptPropertyType::Actor);
            if (!property || !property->ActorValue.IsValid() || !scene.FindActorByID(property->ActorValue))
            {
                return sol::make_object(lua, sol::nil);
            }

            return sol::make_object(lua, CreateActorReference(lua, scene, property->ActorValue));
        });

        const auto installAssetProperty = [&](const char* functionName, const Scene::ScriptPropertyType propertyType, const Project::AssetType assetType)
        {
            exposeTable.set_function(functionName, [&lua, &projectContext, findProperty, propertyType, assetType](const std::string& name) -> sol::object
            {
                const auto* property =findProperty(name, propertyType);
                if (!property || property->AssetValue.empty())
                {
                    return sol::make_object(lua, sol::nil);
                }

                const auto* record = Project::AssetRegistryIO::FindByID(projectContext.GetAssetRegistry(), property->AssetValue);
                if (!record || record->Type != assetType)
                {
                    return sol::make_object(lua, sol::nil);
                }
                return sol::make_object(lua, property->AssetValue);
            });
        };

        installAssetProperty("Mesh", Scene::ScriptPropertyType::MeshAsset, Project::AssetType::Mesh);
        installAssetProperty("Texture", Scene::ScriptPropertyType::TextureAsset, Project::AssetType::Texture);
        installAssetProperty("Audio", Scene::ScriptPropertyType::AudioAsset, Project::AssetType::Audio);
        installAssetProperty("Script", Scene::ScriptPropertyType::ScriptAsset, Project::AssetType::Script);

        environment["Expose"] = exposeTable;
    }
}
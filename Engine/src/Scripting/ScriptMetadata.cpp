#include "Scripting/ScriptMetadata.h"

#include <cstdint>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <string>
#include <utility>

#include <sol/sol.hpp>

#include "Quiet/Project/AssetRegistry.h"
#include "Quiet/Project/AssetType.h"
#include "Quiet/Project/ProjectContext.h"

namespace Quiet::Scripting
{
    bool DiscoverScriptProperties(const Project::ProjectContext& projectContext,
                                  const std::string_view scriptAssetID,
                                  std::vector<Scene::ScriptProperty>& outProperties,
                                  std::string& outError)
    {
        outProperties.clear();
        outError.clear();

        const Project::AssetRecord* record = Project::AssetRegistryIO::FindByID(projectContext.GetAssetRegistry(), scriptAssetID);
        if (!record || record->Type != Project::AssetType::Script)
        {
            outError = "Script asset is not registered.";
            return false;
        }

        if (record->Path.empty())
        {
            outError = "Script asset has no source path.";
            return false;
        }

        std::filesystem::path sourcePath = record->Path;
        if (sourcePath.is_relative())
        {
            sourcePath = projectContext.GetRootPath() / sourcePath;
        }

        sourcePath = sourcePath.lexically_normal();
        std::error_code filesystemError;

        if (!std::filesystem::is_regular_file(sourcePath, filesystemError) || filesystemError)
        {
            outError = "Script source is unavailable.";
            return false;
        }

        sol::state lua;

        lua.open_libraries(
            sol::lib::base,
            sol::lib::coroutine,
            sol::lib::string,
            sol::lib::math,
            sol::lib::table,
            sol::lib::utf8);

        sol::environment environment(lua, sol::create, lua.globals());

        // Property discovery must not spam the editor console
        // because a script happens to print at file scope.
        environment.set_function("print", [](sol::variadic_args) {});

        std::string declarationError;

        const auto alreadyDeclared = [&outProperties](const std::string& name)
        {
            return std::any_of(
                outProperties.begin(),
                outProperties.end(),
                [&name](const Scene::ScriptProperty& property)
                {
                    return property.Name == name;
                });
        };

        sol::table exposeTable = lua.create_table();

        const auto declareReference = [&](const std::string& name, const Scene::ScriptPropertyType type) -> sol::object
        {
            if (name.empty())
            {
                declarationError = "Exposed property names cannot be empty.";
                return sol::make_object(lua, sol::nil);
            }

            if (alreadyDeclared(name))
            {
                declarationError = "Duplicate exposed property '" + name + "'.";
                return sol::make_object(lua, sol::nil);
            }

            Scene::ScriptProperty property{};
            property.Name = name;
            property.Type = type;

            outProperties.push_back(std::move(property));
            return sol::make_object(lua, sol::nil);
        };

        exposeTable.set_function("Float", [&](const std::string& name, const float defaultValue)
        {
            if (name.empty())
            {
                declarationError = "Exposed property names cannot be empty.";
                return defaultValue;
            }

            if (!std::isfinite(defaultValue))
            {
                declarationError = "Exposed Float property '" + name + "' has a non-finite default value.";
                return 0.0f;
            }

            if (alreadyDeclared(name))
            {
                declarationError = "Duplicate exposed property '" + name + "'.";
                return defaultValue;
            }

            Scene::ScriptProperty property{};
            property.Name = name;
            property.Type = Scene::ScriptPropertyType::Float;
            property.FloatValue = defaultValue;

            outProperties.push_back(std::move(property));
            return defaultValue;
        });

        exposeTable.set_function("Int", [&](const std::string& name, const std::int64_t defaultValue)
        {
            if (name.empty())
            {
                declarationError = "Exposed property names cannot be empty.";
                return defaultValue;
            }

            if (alreadyDeclared(name))
            {
                declarationError = "Duplicate exposed property '" + name + "'.";
                return defaultValue;
            }

            Scene::ScriptProperty property{};
            property.Name = name;
            property.Type = Scene::ScriptPropertyType::Int;
            property.IntValue = defaultValue;

            outProperties.push_back(std::move(property));
            return defaultValue;
        });

        exposeTable.set_function("Bool", [&](const std::string& name, const bool defaultValue)
        {
            if (name.empty())
            {
                declarationError = "Exposed property names cannot be empty.";
                return defaultValue;
            }

            if (alreadyDeclared(name))
            {
                declarationError = "Duplicate exposed property '" + name + "'.";
                return defaultValue;
            }

            Scene::ScriptProperty property{};
            property.Name = name;
            property.Type = Scene::ScriptPropertyType::Bool;
            property.BoolValue = defaultValue;

            outProperties.push_back(std::move(property));
            return defaultValue;
        });

        exposeTable.set_function("String", [&](const std::string& name, const std::string& defaultValue)
        {
            if (name.empty())
            {
                declarationError = "Exposed property names cannot be empty.";
                return defaultValue;
            }

            if (alreadyDeclared(name))
            {
                declarationError = "Duplicate exposed property '" + name + "'.";
                return defaultValue;
            }

            Scene::ScriptProperty property{};
            property.Name = name;
            property.Type = Scene::ScriptPropertyType::String;
            property.StringValue = defaultValue;

            outProperties.push_back(std::move(property));
            return defaultValue;
        });

        exposeTable.set_function("Vector2", [&](const std::string& name, const float x, const float y)
        {
            if (name.empty() || !std::isfinite(x) || !std::isfinite(y) || alreadyDeclared(name))
            {
                if (name.empty())
                {
                    declarationError = "Exposed property names cannot be empty.";
                }
                else if (alreadyDeclared(name))
                {
                    declarationError = "Duplicate exposed property '" + name + "'.";
                }
                else
                {
                    declarationError = "Exposed Vector2 property '" + name + "' contains a non-finite value.";
                }
            }
            else
            {
                Scene::ScriptProperty property{};
                property.Name = name;
                property.Type = Scene::ScriptPropertyType::Vector2;
                property.Vector2Value = { x, y };

                outProperties.push_back(std::move(property));
            }

            sol::table value = lua.create_table();
            value["x"] = x;
            value["y"] = y;

            return value;
        });

        exposeTable.set_function("Vector3", [&](const std::string& name, const float x, const float y, const float z)
        {
            if (name.empty() || !std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z) || alreadyDeclared(name))
            {
                if (name.empty())
                {
                    declarationError = "Exposed property names cannot be empty.";
                }
                else if (alreadyDeclared(name))
                {
                    declarationError = "Duplicate exposed property '" + name + "'.";
                }
                else
                {
                    declarationError = "Exposed Vector3 property '" + name + "' contains a non-finite value.";
                }
            }
            else
            {
                Scene::ScriptProperty property{};
                property.Name = name;
                property.Type = Scene::ScriptPropertyType::Vector3;
                property.Vector3Value = { x, y, z };

                outProperties.push_back(std::move(property));
            }

            sol::table value = lua.create_table();
            value["x"] = x;
            value["y"] = y;
            value["z"] = z;

            return value;
        });

        exposeTable.set_function("Vector4", [&](const std::string& name, const float x, const float y, const float z, const float w)
        {
            if (name.empty() || !std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z) || !std::isfinite(w) || alreadyDeclared(name))
            {
                if (name.empty())
                {
                    declarationError = "Exposed property names cannot be empty.";
                }
                else if (alreadyDeclared(name))
                {
                    declarationError = "Duplicate exposed property '" + name + "'.";
                }
                else
                {
                    declarationError = "Exposed Vector4 property '" + name + "' contains a non-finite value.";
                }
            }
            else
            {
                Scene::ScriptProperty property{};
                property.Name = name;
                property.Type = Scene::ScriptPropertyType::Vector4;
                property.Vector4Value = { x, y, z, w };

                outProperties.push_back(std::move(property));
            }

            sol::table value = lua.create_table();
            value["x"] = x;
            value["y"] = y;
            value["z"] = z;
            value["w"] = w;

            return value;
        });

        exposeTable.set_function("Color3", [&](const std::string& name, const float r, const float g, const float b)
        {
            if (name.empty() || !std::isfinite(r) || !std::isfinite(g) || !std::isfinite(b) || alreadyDeclared(name))
            {
                if (name.empty())
                {
                    declarationError = "Exposed property names cannot be empty.";
                }
                else if (alreadyDeclared(name))
                {
                    declarationError = "Duplicate exposed property '" + name + "'.";
                }
                else
                {
                    declarationError = "Exposed Color3 property '" + name + "' contains a non-finite value.";
                }
            }
            else
            {
                Scene::ScriptProperty property{};
                property.Name = name;
                property.Type = Scene::ScriptPropertyType::Color3;
                property.Color3Value = { r, g, b };

                outProperties.push_back(std::move(property));
            }

            sol::table value = lua.create_table();
            value["r"] = r;
            value["g"] = g;
            value["b"] = b;

            return value;
        });

        exposeTable.set_function("Color4", [&](const std::string& name, const float r, const float g, const float b, const float a)
        {
            if (name.empty() || !std::isfinite(r) || !std::isfinite(g) || !std::isfinite(b) || !std::isfinite(a) || alreadyDeclared(name))
            {
                if (name.empty())
                {
                    declarationError = "Exposed property names cannot be empty.";
                }
                else if (alreadyDeclared(name))
                {
                    declarationError = "Duplicate exposed property '" + name + "'.";
                }
                else
                {
                    declarationError = "Exposed Color4 property '" + name + "' contains a non-finite value.";
                }
            }
            else
            {
                Scene::ScriptProperty property{};
                property.Name = name;
                property.Type = Scene::ScriptPropertyType::Color4;
                property.Color4Value = { r, g, b, a };

                outProperties.push_back(std::move(property));
            }

            sol::table value = lua.create_table();
            value["r"] = r;
            value["g"] = g;
            value["b"] = b;
            value["a"] = a;

            return value;
        });

        exposeTable.set_function("Actor", [&](const std::string& name)
        {
            return declareReference(name, Scene::ScriptPropertyType::Actor);
        });

        exposeTable.set_function("Mesh", [&](const std::string& name)
        {
            return declareReference(name, Scene::ScriptPropertyType::MeshAsset);
        });

        exposeTable.set_function("Texture", [&](const std::string& name)
        {
            return declareReference(name, Scene::ScriptPropertyType::TextureAsset);
        });

        exposeTable.set_function("Audio", [&](const std::string& name)
        {
            return declareReference(name, Scene::ScriptPropertyType::AudioAsset);
        });

        exposeTable.set_function("Script", [&](const std::string& name)
        {
            return declareReference(name, Scene::ScriptPropertyType::ScriptAsset);
        });

        environment["Expose"] = exposeTable;

        const sol::protected_function_result result = lua.safe_script_file(sourcePath.string(), environment, sol::script_pass_on_error, sol::load_mode::text);
        if (!result.valid())
        {
            const sol::error error = result;
            outProperties.clear();
            outError = error.what();
            return false;
        }

        if (!declarationError.empty())
        {
            outProperties.clear();
            outError = declarationError;
            return false;
        }

        return true;
    }
}
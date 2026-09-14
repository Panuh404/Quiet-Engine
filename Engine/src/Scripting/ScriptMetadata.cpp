#include "Scripting/ScriptMetadata.h"

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
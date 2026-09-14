#include "Scripting/Bindings/Bindings.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "Scene/Components/ScriptComponent.h"

namespace Quiet::Scripting::Bindings
{
    void InstallPropertyBindings(sol::state& lua, sol::environment& environment, const Scene::ScriptComponent& script)
    {
        const std::vector<Scene::ScriptProperty> properties = script.Properties;

        sol::table exposeTable = lua.create_table();
        exposeTable.set_function("Float", [properties](const std::string& name, const float defaultValue)
        {
            const float safeDefault = std::isfinite(defaultValue) ? defaultValue : 0.0f;
            const auto iterator = std::find_if(
                properties.begin(),
                properties.end(),
                [&name](const Scene::ScriptProperty& property)
                {
                    return property.Name == name && property.Type == Scene::ScriptPropertyType::Float;
                });

            if (iterator == properties.end() || !std::isfinite(iterator->FloatValue)) return safeDefault;
            return iterator->FloatValue;
        });

        exposeTable.set_function("Bool", [properties](const std::string& name, const bool defaultValue)
        {
            const auto iterator = std::find_if(
                properties.begin(),
                properties.end(),
                [&name](const Scene::ScriptProperty& property)
                {
                    return property.Name == name && property.Type == Scene::ScriptPropertyType::Bool;
                });

            if (iterator == properties.end()) return defaultValue;
            return iterator->BoolValue;
        });

        environment["Expose"] = exposeTable;
    }
}
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace Quiet::Scene
{
    enum class ScriptPropertyType : std::uint8_t
    {
        Float = 0,
        Bool
    };

    struct ScriptProperty
    {
        std::string Name;
        ScriptPropertyType Type = ScriptPropertyType::Float;

        float FloatValue = 0.0f;
        bool BoolValue = false;

        bool operator==(const ScriptProperty&) const = default;
    };

    struct ScriptComponent
    {
        std::string ScriptAssetID;
        std::vector<ScriptProperty> Properties;
    };
}
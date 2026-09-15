#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include "Quiet/Core/UUID.h"

namespace Quiet::Scene
{
    enum class ScriptPropertyType : std::uint8_t
    {
        Float = 0,
        Int,
        Bool,
        String,

        Vector2,
        Vector3,
        Vector4,

        Color3,
        Color4,

        Actor,

        MeshAsset,
        TextureAsset,
        AudioAsset,
        ScriptAsset
    };

    struct ScriptProperty
    {
        std::string Name;
        ScriptPropertyType Type = ScriptPropertyType::Float;

        // Properties
        float FloatValue = 0.0f;
        std::int64_t IntValue = 0;
        bool BoolValue = false;
        std::string StringValue;

        glm::vec2 Vector2Value{0.0f};
        glm::vec3 Vector3Value{0.0f};
        glm::vec4 Vector4Value{0.0f};

        glm::vec3 Color3Value{1.0f};
        glm::vec4 Color4Value{1.0f};

        Core::UUID ActorValue{};
        std::string AssetValue;

        bool operator==(const ScriptProperty&) const = default;
    };

    struct ScriptComponent
    {
        std::string ScriptAssetID;
        std::vector<ScriptProperty> Properties;
    };
}
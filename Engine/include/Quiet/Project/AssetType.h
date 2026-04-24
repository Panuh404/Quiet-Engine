#pragma once

#include <string_view>

namespace Quiet::Project
{
    enum class AssetType
    {
        Unknown = 0,
        Mesh
    };

    [[nodiscard]] constexpr std::string_view ToString(const AssetType type)
    {
        switch (type)
        {
            case AssetType::Mesh: return "Mesh";
            case AssetType::Unknown:
            default:
                return "Unknown";
        }
    }

    inline bool AssetTypeFromString(const std::string_view value, AssetType& outType)
    {
        if (value == "Mesh")
        {
            outType = AssetType::Mesh;
            return true;
        }

        if (value == "Unknown")
        {
            outType = AssetType::Unknown;
            return true;
        }

        return false;
    }
}


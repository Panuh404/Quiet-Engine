#pragma once

#include <string_view>

namespace Quiet::Project
{
    enum class AssetType
    {
        Unknown = 0,
        Mesh,
        Texture,
        Audio,
        Script,
        UIDocument,
        UIStyleSheet
    };

    [[nodiscard]] constexpr std::string_view ToString(const AssetType type)
    {
        switch (type)
        {
            case AssetType::Mesh: return "Mesh";
            case AssetType::Texture: return "Texture";
            case AssetType::Audio: return "Audio";
            case AssetType::Script: return "Script";
            case AssetType::UIDocument: return "UIDocument";
            case AssetType::UIStyleSheet: return "UIStyleSheet";
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
        if (value == "Texture")
        {
            outType = AssetType::Texture;
            return true;
        }
        if (value == "Audio")
        {
            outType = AssetType::Audio;
            return true;
        }
        if (value == "Script")
        {
            outType = AssetType::Script;
            return true;
        }
        if (value == "Unknown")
        {
            outType = AssetType::Unknown;
            return true;
        }
        if (value == "UIDocument")
        {
            outType = AssetType::UIDocument;
            return true;
        }
        if (value == "UIStyleSheet")
        {
            outType = AssetType::UIStyleSheet;
            return true;
        }
        return false;
    }
}


#pragma once

#include <string>

#include "Quiet/Project/AssetType.h"

namespace Quiet::Project
{
    class ProjectContext;
}

namespace Quiet::Renderer::Vulkan
{
    class MeshLibrary;
    class TextureLibrary;
}

namespace Quiet::Renderer::UI::Assets
{
    struct AssetPickerOptions
    {
        Project::AssetType Type = Project::AssetType::Unknown;
        bool AllowNone = false;
        const Vulkan::MeshLibrary* MeshLibrary = nullptr;
        const Vulkan::TextureLibrary* TextureLibrary = nullptr;
    };

    [[nodiscard]] bool DrawAssetPicker(
        const char* id,
        const Project::ProjectContext& projectContext,
        const AssetPickerOptions& options,
        const std::string& currentAssetID,
        std::string& outSelectedAssetID
    );
}
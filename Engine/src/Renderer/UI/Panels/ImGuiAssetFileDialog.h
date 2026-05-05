#pragma once

#include <filesystem>
#include <optional>
#include <string>

#include "Quiet/Project/AssetType.h"

namespace Quiet::Project
{
    class ProjectContext;
}

namespace Quiet::Renderer::UI
{
    struct AssetFileDialogResult
    {
        Project::AssetType Type = Project::AssetType::Unknown;
        std::string AssetID;
        std::string AssetPath;
    };

    [[nodiscard]] Project::AssetType DetectImportAssetType(const std::filesystem::path& path);

    void OpenAssetFileDialog(const Project::ProjectContext& projectContext);

    [[nodiscard]] std::optional<AssetFileDialogResult> DrawAssetFileDialog(
        const Project::ProjectContext& projectContext
    );
}
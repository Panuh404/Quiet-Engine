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
        std::string SourcePath;
    };

    [[nodiscard]] Project::AssetType DetectImportAssetType(const std::filesystem::path& path);

    void OpenAssetFileDialog(const Project::ProjectContext& projectContext, const std::string& initialAssetFolder);

    [[nodiscard]] bool IsAssetFileDialogOpen();
    [[nodiscard]] std::optional<AssetFileDialogResult> DrawAssetFileDialog();
}
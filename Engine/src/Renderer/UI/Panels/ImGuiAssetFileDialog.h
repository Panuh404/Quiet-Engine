#pragma once

#include <optional>
#include <string>

namespace Quiet::Project
{
    class ProjectContext;
}

namespace Quiet::Renderer::UI
{
    struct MeshAssetFileDialogResult
    {
        std::string AssetID;
        std::string AssetPath;
    };

    void OpenMeshAssetFileDialog(const Project::ProjectContext& projectContext);

    [[nodiscard]] std::optional<MeshAssetFileDialogResult> DrawMeshAssetFileDialog(
        const Project::ProjectContext& projectContext
    );
}
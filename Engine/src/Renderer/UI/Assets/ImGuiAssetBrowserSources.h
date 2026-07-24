#pragma once

#include <string>

#include "Renderer/UI/Assets/ImGuiAssetBrowserTypes.h"
#include "Quiet/Project/ProjectContext.h"

namespace Quiet::Renderer::UI::Assets
{
    void DrawAssetBrowserSourcesPanel(
        const Project::ProjectContext& projectContext,
        std::string& currentAssetFolder,
        float width,
        bool runtimeModeActive,
        AssetBrowserState& state,
        AssetBrowserFolderActionRequest& folderActionRequest,
        AssetBrowserAssetMoveRequest& assetMoveRequest,
        AssetBrowserOperationRequest& operationRequest
    );
}
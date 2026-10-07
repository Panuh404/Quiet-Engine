#pragma once

#include <string>

#include "Renderer/UI/Assets/ImGuiAssetBrowserTypes.h"
#include "Quiet/Project/AssetRegistry.h"
#include "Quiet/Project/ProjectContext.h"

namespace Quiet::Renderer::UI::Assets
{
    [[nodiscard]] bool IsAssetBrowserContextMenuRequestedForLastItem();

    void DrawAssetContextMenu(
        const char* popupID,
        const Project::ProjectContext& projectContext,
        const Project::AssetRecord& record,
        bool runtimeModeActive,
        AssetBrowserState& state,
        AssetBrowserOperationRequest& operationRequest,
        std::string& pendingDetailsAssetID,
        std::string& pendingReimportAssetID
    );

    void DrawAssetFolderContextMenu(
        const char* popupID,
        const Project::ProjectContext& projectContext,
        const std::string& relativeFolderPath,
        bool runtimeModeActive,
        AssetBrowserState& state,
        AssetBrowserOperationRequest& operationRequest,
        AssetBrowserFolderActionRequest& folderActionRequest
    );

    void DrawAssetBrowserBackgroundContextMenu(
        const char* popupID,
        const Project::ProjectContext& projectContext,
        const std::string& currentAssetFolder,
        bool runtimeModeActive,
        AssetBrowserState& state,
        AssetBrowserOperationRequest& operationRequest,
        AssetBrowserFolderActionRequest& folderActionRequest
    );
}
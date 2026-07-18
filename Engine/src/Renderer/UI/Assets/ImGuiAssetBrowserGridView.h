#pragma once

#include <string>
#include <vector>

#include "Quiet/Editor/EditorState.h"
#include "Quiet/Project/AssetRegistry.h"
#include "Quiet/Project/ProjectContext.h"
#include "Renderer/UI/Assets/ImGuiAssetBrowserTypes.h"
#include "Renderer/Vulkan/Texture/TextureLibrary.h"
#include "Scene/Scene.h"

namespace Quiet::Renderer::UI::Assets
{
    void DrawAssetBrowserGrid(
        const std::vector<AssetBrowserFolderEntry>& visibleFolders,
        const std::vector<const Project::AssetRecord*>& visibleAssets,
        const Project::ProjectContext& projectContext,
        Editor::EditorState& editorState,
        const Scene::Scene& scene,
        const Vulkan::TextureLibrary& textureLibrary,
        AssetBrowserState& state,
        std::string& pendingNavigateFolder,
        AssetBrowserFolderActionRequest& folderActionRequest,
        AssetBrowserAssetMoveRequest& assetMoveRequest,
        AssetBrowserOperationRequest& operationRequest,
        std::string& pendingDetailsAssetID,
        std::string& pendingReimportAssetID,
        std::string& pendingRemoveAssetID,
        AssetBrowserThumbnailSize thumbnailSize,
        float uiScale
    );
}
#pragma once

#include <string>
#include <vector>

#include "ImGuiAssetBrowserTypes.h"
#include "Quiet/Editor/EditorState.h"
#include "Quiet/Project/AssetRegistry.h"
#include "Quiet/Project/ProjectContext.h"
#include "Renderer/Vulkan/Mesh/MeshLibrary.h"
#include "Renderer/Vulkan/Texture/TextureLibrary.h"
#include "Scene/Scene.h"

namespace Quiet::Renderer::UI::Assets
{
    void DrawAssetBrowserList(
        const std::vector<AssetBrowserFolderEntry>& visibleFolders,
        const std::vector<const Project::AssetRecord*>& visibleAssets,
        const Project::ProjectContext& projectContext,
        Editor::EditorState& editorState,
        const Scene::Scene& scene,
        const Vulkan::MeshLibrary& meshLibrary,
        const Vulkan::TextureLibrary& textureLibrary,
        std::string& selectedContentFolder,
        std::string& selectedDetailsAssetID,
        std::string& pendingNavigateFolder,
        AssetBrowserFolderActionRequest& folderActionRequest,
        AssetBrowserAssetMoveRequest& assetMoveRequest,
        std::string& pendingDetailsAssetID,
        std::string& pendingReimportAssetID,
        std::string& pendingRemoveAssetID
    );
}
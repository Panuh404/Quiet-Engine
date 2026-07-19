#pragma once

#include <string>
#include <vector>

#include "Scene/Scene.h"
#include "Renderer/UI/Assets/ImGuiAssetBrowserTypes.h"
#include "Quiet/Editor/EditorState.h"
#include "Quiet/Project/ProjectContext.h"
#include "Quiet/Project/AssetRecord.h"

namespace Quiet::Renderer::UI::Assets
{
    void DrawFailurePopup(const char* popupTitle, std::string& errorMessage);
    void DrawAssetOperationFeedback(Editor::EditorState& editorState);

    void RequestAssetRegistryRemoval(AssetBrowserState& state, const std::vector<std::string>& assetIDs);
    void DrawRemoveAssetConfirmationPopup(
        Project::ProjectContext& projectContext,
        Editor::EditorState& editorState,
        const Scene::Scene& scene,
        AssetBrowserState& state,
        float uiScale
    );
    // Folder Stuff
    [[nodiscard]] const char* GetAssetFolderActionPopupTitle(AssetBrowserFolderAction action);
    void BeginAssetFolderAction(AssetBrowserState& state, const AssetBrowserFolderActionRequest& request);
    void DrawAssetFolderActionPopup(Project::ProjectContext& projectContext, Editor::EditorState& editorState, AssetBrowserState& state);

    void RequestAssetRename(AssetBrowserState& state, const Project::AssetRecord& record);
    void DrawAssetRenamePopup(Project::ProjectContext& projectContext, Editor::EditorState& editorState, AssetBrowserState& state);

}
#pragma once

#include <string>

#include "ImGuiAssetBrowserTypes.h"
#include "Quiet/Editor/EditorState.h"
#include "Quiet/Project/ProjectContext.h"

namespace Quiet::Renderer::UI::Assets
{
    void DrawFailurePopup(const char* popupTitle, std::string& errorMessage);
    void DrawAssetOperationFeedback(Editor::EditorState& editorState);

    void DrawRemoveAssetConfirmationPopup(const Project::ProjectContext& projectContext, const std::string& assetID, bool& confirmed, bool& cancelled, float uiScale);

    // Folder Stuff
    [[nodiscard]] const char* GetAssetFolderActionPopupTitle(AssetBrowserFolderAction action);
    void BeginAssetFolderAction(AssetBrowserState& state, const AssetBrowserFolderActionRequest& request);
    void DrawAssetFolderActionPopup(Project::ProjectContext& projectContext, Editor::EditorState& editorState, AssetBrowserState& state);

}
#pragma once

#include <string>

#include "Quiet/Editor/EditorState.h"
#include "Quiet/Project/ProjectContext.h"

namespace Quiet::Renderer::UI::Assets
{
    void DrawFailurePopup(const char* popupTitle, std::string& errorMessage);

    void DrawAssetOperationFeedback(Editor::EditorState& editorState);

    void DrawRemoveAssetConfirmationPopup(
        const Project::ProjectContext& projectContext,
        const std::string& assetID,
        bool& confirmed,
        bool& cancelled,
        float uiScale
    );
}
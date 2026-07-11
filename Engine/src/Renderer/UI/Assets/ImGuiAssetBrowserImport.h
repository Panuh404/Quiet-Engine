#pragma once

#include <array>
#include <string>

#include "Quiet/Editor/EditorState.h"
#include "Quiet/Project/ProjectContext.h"

namespace Quiet::Renderer::UI::Assets
{
    bool DrawRegisterAssetForm(
        Project::ProjectContext& projectContext,
        Editor::EditorState& editorState,
        const std::string& currentAssetFolder,
        std::array<char, 128>& assetIDBuffer,
        std::array<char, 1024>& assetPathBuffer,
        std::string& errorMessage,
        bool& reopenImportPopupAfterFileDialog
    );

    void DrawImportAssetPopup(
        Project::ProjectContext& projectContext,
        Editor::EditorState& editorState,
        const std::string& currentAssetFolder,
        std::array<char, 128>& assetIDBuffer,
        std::array<char, 1024>& assetPathBuffer,
        std::string& errorMessage,
        bool& reopenImportPopupAfterFileDialog
    );
}
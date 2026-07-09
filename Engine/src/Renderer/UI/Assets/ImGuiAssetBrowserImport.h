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
        std::array<char, 128>& assetIDBuffer,
        std::array<char, 260>& assetPathBuffer,
        std::string& errorMessage
    );

    void DrawImportAssetPopup(
        Project::ProjectContext& projectContext,
        Editor::EditorState& editorState,
        std::array<char, 128>& assetIDBuffer,
        std::array<char, 260>& assetPathBuffer,
        std::string& errorMessage
    );
}
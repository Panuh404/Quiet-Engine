#pragma once

#include "Quiet/Editor/EditorState.h"
#include "Quiet/Project/ProjectContext.h"
#include "Renderer/UI/Assets/ImGuiAssetBrowserTypes.h"

namespace Quiet::Renderer::UI::Assets
{
    [[nodiscard]] bool ExecuteAssetMoveOperation(
        Project::ProjectContext& projectContext,
        Editor::EditorState& editorState,
        AssetBrowserState& state,
        const AssetBrowserAssetMoveRequest& request
    );
}
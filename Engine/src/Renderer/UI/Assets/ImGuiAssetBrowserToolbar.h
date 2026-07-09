#pragma once

#include <array>

#include "Quiet/Editor/EditorState.h"
#include "Quiet/Project/AssetRegistry.h"
#include "Renderer/UI/Assets/ImGuiAssetBrowserTypes.h"

namespace Quiet::Renderer::UI::Assets
{
    void DrawAssetBrowserToolbar(
        Editor::EditorState& editorState,
        const Project::AssetRegistry& registry,
        std::array<char, 128>& searchBuffer,
        AssetBrowserTypeFilter& typeFilter,
        AssetBrowserViewMode& viewMode,
        AssetBrowserThumbnailSize& thumbnailSize,
        bool& openImportPopup
    );
}
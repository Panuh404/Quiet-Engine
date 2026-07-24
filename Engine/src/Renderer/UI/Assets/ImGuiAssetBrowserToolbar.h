#pragma once

#include <array>

#include "Quiet/Editor/EditorState.h"
#include "Quiet/Project/ProjectContext.h"
#include "Renderer/UI/Assets/ImGuiAssetBrowserTypes.h"

namespace Quiet::Renderer::Vulkan
{
    class MeshLibrary;
}

namespace Quiet::Renderer::UI::Assets
{
    void DrawAssetBrowserToolbar(
        Editor::EditorState& editorState,
        const Project::ProjectContext& projectContext,
        const Vulkan::MeshLibrary& meshLibrary,
        std::array<char, 128>& searchBuffer,
        AssetBrowserTypeFilter& typeFilter,
        AssetBrowserDiagnosticsFilter& diagnosticsFilter,
        AssetBrowserViewMode& viewMode,
        AssetBrowserThumbnailSize& thumbnailSize,
        AssetBrowserState& state,
        AssetBrowserOperationRequest& operationRequest,
        bool& openImportPopup
    );
}
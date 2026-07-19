#pragma once

#include "Quiet/Editor/EditorState.h"
#include "Quiet/Project/ProjectContext.h"
#include "Renderer/UI/Assets/ImGuiAssetBrowserTypes.h"
#include "Scene/Scene.h"

namespace Quiet::Renderer::UI::Assets
{
    [[nodiscard]] bool ExecuteAssetMoveOperation(
        Project::ProjectContext& projectContext,
        Editor::EditorState& editorState,
        AssetBrowserState& state,
        const AssetBrowserAssetMoveRequest& request
    );

    [[nodiscard]] bool ExecuteAssetDuplicateOperation(
        Project::ProjectContext& projectContext,
        Editor::EditorState& editorState,
        AssetBrowserState& state,
        const AssetBrowserOperationRequest& request
    );

    [[nodiscard]] bool ExecuteAssetRenameOperation(
        Project::ProjectContext& projectContext,
        Editor::EditorState& editorState,
        AssetBrowserState& state,
        const AssetBrowserOperationRequest& request
    );

    [[nodiscard]] bool ExecuteAssetRemoveFromRegistryOperation(
        Project::ProjectContext& projectContext,
        Editor::EditorState& editorState,
        AssetBrowserState& state,
        const Scene::Scene& scene,
        const AssetBrowserOperationRequest& request
    );

    [[nodiscard]] bool IsAssetClipboardAvailable(
        const Project::ProjectContext& projectContext,
        const AssetBrowserState& state
    );

    void SynchronizeAssetClipboard(
        const Project::ProjectContext& projectContext,
        AssetBrowserState& state
    );

    [[nodiscard]] bool ExecuteAssetBrowserOperation(
        Project::ProjectContext& projectContext,
        Editor::EditorState& editorState,
        AssetBrowserState& state,
        const AssetBrowserOperationRequest& request,
        const Scene::Scene* scene = nullptr
    );
}
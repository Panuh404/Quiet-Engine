#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "Quiet/Editor/EditorState.h"
#include "Quiet/Project/ProjectContext.h"
#include "Renderer/UI/Assets/ImGuiAssetBrowserTypes.h"
#include "Scene/Scene.h"

namespace Quiet::Renderer::UI::Assets
{
    struct AssetFolderDeleteSummary
    {
        std::string Folder{};
        std::vector<std::string> RegisteredAssetIDs{};

        std::uint32_t PhysicalFileCount = 0;
        std::uint32_t NestedFolderCount = 0;
        std::uint32_t UnregisteredFileCount = 0;
        std::uint32_t MissingRegisteredAssetCount = 0;

        std::uint32_t ReferencedAssetCount = 0;
        std::uint32_t SceneReferenceCount = 0;

        std::uint32_t SymbolicLinkCount = 0;
        std::uint32_t UnsupportedEntryCount = 0;

        std::string ErrorMessage{};

        [[nodiscard]] bool IsValid() const
        {
            return !Folder.empty() && ErrorMessage.empty();
        }

        [[nodiscard]] bool CanDelete() const
        {
            return IsValid() && ReferencedAssetCount == 0 && SymbolicLinkCount == 0 && UnsupportedEntryCount == 0;
        }
    };

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

    [[nodiscard]] AssetFolderDeleteSummary InspectAssetFolderForDeletion(
        const Project::ProjectContext& projectContext,
        const Scene::Scene& scene,
        const std::string& folder
    );

    [[nodiscard]] bool ExecuteAssetFolderDeleteOperation(
        Project::ProjectContext& projectContext,
        Editor::EditorState& editorState,
        AssetBrowserState& state,
        const Scene::Scene& scene,
        const std::string& folder
    );

    [[nodiscard]] bool ExecuteAssetDeleteFromProjectOperation(
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
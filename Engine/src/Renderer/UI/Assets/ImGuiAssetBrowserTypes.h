#pragma once
#include <array>
#include <cstddef>
#include <string>
#include <vector>

namespace Quiet::Renderer::UI::Assets
{
    enum class AssetBrowserTypeFilter
    {
        All,
        Meshes,
        Textures
    };

    enum class AssetBrowserViewMode
    {
        List,
        Browser
    };

    enum class AssetBrowserThumbnailSize
    {
        Small,
        Medium,
        Large
    };

    enum class AssetBrowserFolderAction
    {
        None,
        Create,
        Rename,
        Delete
    };

    enum class AssetBrowserDiagnosticsFilter
    {
        All,
        Issues,
        Missing,
        External
    };

    struct AssetBrowserFolderActionRequest
    {
        AssetBrowserFolderAction Action = AssetBrowserFolderAction::None;
        std::string TargetFolder{};
    };

    struct AssetBrowserFolderEntry
    {
        std::string Name{};
        std::string RelativePath{};
    };

    struct AssetBrowserAssetMoveRequest
    {
        std::string AssetID{};
        std::string TargetFolder{};

        [[nodiscard]] bool IsValid() const
        {
            return !AssetID.empty() && !TargetFolder.empty();
        }
    };

    struct AssetBrowserState
    {
        std::array<char, 128> AssetIDBuffer{};
        std::array<char, 1024> AssetPathBuffer{};
        std::array<char, 128> FolderNameBuffer{};
        std::array<char, 128> SearchBuffer{};

        std::string ImportErrorMessage{};
        std::string RemoveErrorMessage{};
        std::string SelectedDetailsAssetID{};
        std::string ConfirmRemoveAssetID{};
        std::string CurrentAssetFolder{};
        std::string SelectedContentFolder{};

        std::vector<std::string> FolderHistory{};
        std::size_t FolderHistoryIndex = 0;

        std::string FolderOperationTarget{};
        std::string FolderOperationErrorMessage{};


        bool ReopenImportPopupAfterFileDialog = false;

        AssetBrowserTypeFilter TypeFilter = AssetBrowserTypeFilter::All;
        AssetBrowserViewMode ViewMode = AssetBrowserViewMode::Browser;
        AssetBrowserThumbnailSize ThumbnailSize = AssetBrowserThumbnailSize::Medium;
        AssetBrowserFolderAction PendingFolderAction = AssetBrowserFolderAction::None;
        AssetBrowserDiagnosticsFilter DiagnosticsFilter = AssetBrowserDiagnosticsFilter::All;
    };
}
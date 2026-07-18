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
        std::vector<std::string> AssetIDs{};
        std::string TargetFolder{};

        [[nodiscard]] bool IsValid() const
        {
            return !AssetIDs.empty() && !TargetFolder.empty();
        }
    };

    enum class AssetBrowserClipboardMode
    {
        None,
        Copy,
        Move
    };

    struct AssetBrowserClipboardState
    {
        AssetBrowserClipboardMode Mode = AssetBrowserClipboardMode::None;
        std::vector<std::string> AssetIDs{};
        std::string ProjectRoot{};

        [[nodiscard]] bool IsValid() const
        {
            return Mode != AssetBrowserClipboardMode::None && !AssetIDs.empty() && !ProjectRoot.empty();
        }

        void Clear()
        {
            Mode = AssetBrowserClipboardMode::None;
            AssetIDs.clear();
            ProjectRoot.clear();
        }
    };

    enum class AssetBrowserOperationAction
    {
        None,
        CopySelection,
        CutSelection,
        Paste,
        Duplicate
    };

    struct AssetBrowserOperationRequest
    {
        AssetBrowserOperationAction Action = AssetBrowserOperationAction::None;
        std::vector<std::string> AssetIDs{};
        std::string TargetFolder{};

        [[nodiscard]] bool IsValid() const
        {
            switch (Action)
            {
                case AssetBrowserOperationAction::CopySelection:
                case AssetBrowserOperationAction::CutSelection:
                    return !AssetIDs.empty();

                case AssetBrowserOperationAction::Paste:
                    return !TargetFolder.empty();

                case AssetBrowserOperationAction::Duplicate:
                    return !AssetIDs.empty() && !TargetFolder.empty();

                case AssetBrowserOperationAction::None:
                default:
                    return false;
            }
        }

        void Clear()
        {
            Action = AssetBrowserOperationAction::None;
            AssetIDs.clear();
            TargetFolder.clear();
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

        std::vector<std::string> SelectedAssetIDs{};
        std::string SelectionAnchorAssetID{};
        AssetBrowserClipboardState Clipboard{};

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
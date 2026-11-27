#pragma once
#include <array>
#include <cstddef>
#include <string>
#include <vector>

#include "Quiet/Core/UUID.h"

namespace Quiet::Renderer::UI::Assets
{
    enum class AssetBrowserTypeFilter
    {
        All,
        Meshes,
        Textures,
        Audio,
        Scripts
    };

    enum class AssetBrowserViewMode
    {
        List,
        Browser
    };

    enum class AssetBrowserSource
    {
        Project,
        EngineBuiltIn
    };

    enum class AssetBrowserThumbnailSize
    {
        Tiny,
        Small,
        Medium,
        Large,
        ExtraLarge
    };

    enum class AssetBrowserFolderAction
    {
        None,
        Create,
        CreateScript,
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
        std::vector<Quiet::Core::UUID> AssetIDs{};
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
        std::vector<Quiet::Core::UUID> AssetIDs{};
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
        Duplicate,
        Rename,
        RemoveFromRegistry,
        DeleteFromProject
    };

    struct AssetBrowserOperationRequest
    {
        AssetBrowserOperationAction Action = AssetBrowserOperationAction::None;
        std::vector<Quiet::Core::UUID> AssetIDs{};
        std::string TargetFolder{};
        std::string NewName{};

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

                case AssetBrowserOperationAction::Rename:
                    return AssetIDs.size() == 1 && !NewName.empty();

                case AssetBrowserOperationAction::RemoveFromRegistry:
                    return !AssetIDs.empty();

                case AssetBrowserOperationAction::DeleteFromProject:
                    return !AssetIDs.empty();

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
            NewName.clear();
        }
    };

    struct AssetBrowserState
    {
        std::array<char, 128> AssetNameBuffer{};
        std::array<char, 1024> AssetPathBuffer{};
        std::array<char, 128> FolderNameBuffer{};
        std::array<char, 128> SearchBuffer{};
        std::array<char, 128> AssetRenameBuffer{};

        std::string ImportErrorMessage{};
        std::string ImportTargetFolder{};
        std::string RemoveErrorMessage{};
        std::string DeleteErrorMessage{};
        std::string AssetRenameErrorMessage{};
        Quiet::Core::UUID RenameAssetID{};

        Quiet::Core::UUID SelectedDetailsAssetID{};
        std::vector<Quiet::Core::UUID> ConfirmRemoveAssetIDs{};
        std::vector<Quiet::Core::UUID> ConfirmDeleteAssetIDs{};
        std::string CurrentAssetFolder{};
        std::string SelectedContentFolder{};

        std::vector<Quiet::Core::UUID> SelectedAssetIDs{};
        Quiet::Core::UUID SelectionAnchorAssetID{};
        AssetBrowserClipboardState Clipboard{};

        std::vector<std::string> FolderHistory{};
        std::size_t FolderHistoryIndex = 0;

        std::string FolderOperationTarget{};
        std::string FolderOperationErrorMessage{};

        bool RequestOpenImportPopup = false;
        bool ReopenImportPopupAfterFileDialog = false;
        bool RequestOpenAssetRenamePopup = false;
        bool RequestOpenRemovePopup = false;
        bool RequestOpenDeletePopup = false;
        bool ConfirmPermanentAssetDelete = false;
        bool ConfirmPermanentFolderDelete = false;

        bool SourcesPanelExpanded = true;
        bool DetailsPanelExpanded = true;

        AssetBrowserTypeFilter TypeFilter = AssetBrowserTypeFilter::All;
        AssetBrowserViewMode ViewMode = AssetBrowserViewMode::Browser;
        AssetBrowserThumbnailSize ThumbnailSize = AssetBrowserThumbnailSize::Medium;
        AssetBrowserFolderAction PendingFolderAction = AssetBrowserFolderAction::None;
        AssetBrowserDiagnosticsFilter DiagnosticsFilter = AssetBrowserDiagnosticsFilter::All;
        AssetBrowserSource CurrentSource = AssetBrowserSource::Project;
    };
}
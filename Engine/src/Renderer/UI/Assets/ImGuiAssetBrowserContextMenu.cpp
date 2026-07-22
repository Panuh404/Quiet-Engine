#include "Renderer/UI/Assets/ImGuiAssetBrowserContextMenu.h"

#include <filesystem>
#include <string>
#include <imgui.h>

#include "Quiet/Platform/Shell.h"
#include "Renderer/UI/Assets/ImGuiAssetBrowserUtils.h"
#include "Renderer/UI/Assets/ImGuiAssetBrowserOperations.h"
#include "Renderer/UI/Assets/ImGuiAssetBrowserDialogs.h"
#include "Renderer/UI/ImGuiTheme.h"

namespace
{
    namespace SZ = Quiet::Renderer::UI::EditorSizing;
    namespace QUI = Quiet::Renderer::UI;
    namespace Assets = Quiet::Renderer::UI::Assets;

    std::string MakeAssetReimportDisabledReason(
        const Quiet::Project::AssetRecord& record,
        const bool runtimeModeActive,
        const bool resolvedPathExists)
    {
        if (record.BuiltIn)         return "Built-in assets cannot be reimported.";
        if (runtimeModeActive)      return "Assets cannot be reimported while runtime mode is active.";
        if (record.Path.empty())    return "This asset has no source path.";
        if (!resolvedPathExists)    return "The source file does not exist on disk.";
        return {};
    }

    void RequestFolderAction(
        Assets::AssetBrowserFolderActionRequest& request,
        const Assets::AssetBrowserFolderAction action,
        const std::string& targetFolder)
    {
        request.Action = action;
        request.TargetFolder = targetFolder;
    }

    void DrawFolderContextMenuContents(
        const Quiet::Project::ProjectContext& projectContext,
        const std::string& relativeFolderPath,
        const bool runtimeModeActive,
        const Assets::AssetBrowserState& state,
        Assets::AssetBrowserOperationRequest& operationRequest,
        Assets::AssetBrowserFolderActionRequest& folderActionRequest)
    {
        const std::string assetsRoot = Assets::GetAssetsRootRelativePath(projectContext);
        const bool isAssetsRoot = relativeFolderPath == assetsRoot;

        const bool clipboardAvailable = Assets::IsAssetClipboardAvailable(projectContext, state);
        const bool canPaste = !runtimeModeActive && clipboardAvailable;

        const bool copying = state.Clipboard.Mode == Assets::AssetBrowserClipboardMode::Copy;
        const char* pasteLabel = copying ? "Paste Copies Here" : "Move Assets Here";

        if (QUI::EditorMenuItem(pasteLabel, "Ctrl+V", false, canPaste))
        {
            operationRequest.Clear();
            operationRequest.Action = Assets::AssetBrowserOperationAction::Paste;
            operationRequest.TargetFolder = relativeFolderPath;
        }

        ImGui::Separator();

        if (QUI::EditorMenuItem("Create Folder", nullptr, false, !runtimeModeActive))
        {
            RequestFolderAction(folderActionRequest, Assets::AssetBrowserFolderAction::Create, relativeFolderPath);
        }

        ImGui::Separator();

        if (QUI::EditorMenuItem("Rename", nullptr, false, !runtimeModeActive && !isAssetsRoot))
        {
            RequestFolderAction(folderActionRequest, Assets::AssetBrowserFolderAction::Rename, relativeFolderPath);
        }

        if (QUI::EditorMenuItem("Delete Folder...", nullptr, false, !runtimeModeActive && !isAssetsRoot))
        {
            RequestFolderAction(folderActionRequest, Assets::AssetBrowserFolderAction::Delete, relativeFolderPath);
        }
    }
}

namespace Quiet::Renderer::UI::Assets
{
    bool IsAssetBrowserContextMenuRequestedForLastItem()
    {
        return  ImGui::IsMouseReleased(ImGuiMouseButton_Right) &&
                ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByPopup);
    }

    void DrawAssetContextMenu(
        const char* popupID,
        const Project::ProjectContext& projectContext,
        const Project::AssetRecord& record,
        const bool runtimeModeActive,
        AssetBrowserState& state,
        AssetBrowserOperationRequest& operationRequest,
        std::string& pendingDetailsAssetID,
        std::string& pendingReimportAssetID)
    {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, SZ::Px(ImVec2(12.0f, 10.0f)));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, SZ::Px(ImVec2(8.0f, 6.0f)));
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, SZ::Px(ImVec2(8.0f, 5.0f)));

        if (!ImGui::BeginPopupContextItem(popupID))
        {
            ImGui::PopStyleVar(3);
            return;
        }

        const std::size_t selectedAssetCount = state.SelectedAssetIDs.size();
        const bool canModifySelection = !runtimeModeActive && selectedAssetCount > 0;

        if (EditorMenuItem("Inspect"))
            pendingDetailsAssetID = record.ID;

        ImGui::Separator();

        const char* copyLabel = selectedAssetCount == 1 ? "Copy Asset" : "Copy Selection";
        if (EditorMenuItem(copyLabel,  "Ctrl+C", false, canModifySelection))
        {
            operationRequest.Clear();
            operationRequest.Action = AssetBrowserOperationAction::CopySelection;
            operationRequest.AssetIDs = state.SelectedAssetIDs;
        }

        const char* cutLabel = selectedAssetCount == 1 ? "Cut Asset" : "Cut Selection";
        if (EditorMenuItem(cutLabel, "Ctrl+X", false, canModifySelection))
        {
            operationRequest.Clear();
            operationRequest.Action = AssetBrowserOperationAction::CutSelection;
            operationRequest.AssetIDs = state.SelectedAssetIDs;
        }

        const AssetPathDiagnostics renameDiagnostics = InspectAssetPath(projectContext, record);
        const bool canRename = !runtimeModeActive && selectedAssetCount == 1 && !record.BuiltIn && !renameDiagnostics.IsMissing() && !renameDiagnostics.External;

        if (EditorMenuItem("Rename", "F2", false, canRename))
        {
            RequestAssetRename(state, record);
        }

        if (!canRename && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
        {
            if (selectedAssetCount != 1)
                ImGui::SetTooltip("Rename requires exactly one selected asset.");

            else if (record.BuiltIn)
                ImGui::SetTooltip("Built-in assets cannot be renamed.");

            else if (renameDiagnostics.External)
                ImGui::SetTooltip("External assets cannot be renamed through the project Assets browser.");

            else
                ImGui::SetTooltip("The asset source file is unavailable.");
        }

        const char* duplicateLabel = selectedAssetCount == 1 ? "Duplicate" : "Duplicate Selection";
        if (EditorMenuItem(duplicateLabel, "Ctrl+D", false, canModifySelection))
        {
            operationRequest.Clear();
            operationRequest.Action = AssetBrowserOperationAction::Duplicate;
            operationRequest.AssetIDs = state.SelectedAssetIDs;
            operationRequest.TargetFolder = state.CurrentAssetFolder;
        }

        ImGui::Separator();

        if (EditorMenuItem("Copy ID"))
            ImGui::SetClipboardText(record.ID.c_str());

        const bool hasPath = !record.Path.empty();

        if (EditorMenuItem("Copy Path", nullptr, false, hasPath))
            ImGui::SetClipboardText(record.Path.c_str());

        const std::filesystem::path resolvedPath = hasPath ? ResolveDisplayPath(projectContext, record.Path) : std::filesystem::path{};
        const bool resolvedPathExists = hasPath && !resolvedPath.empty() && std::filesystem::exists(resolvedPath);

        if (EditorMenuItem("Copy Absolute Path", nullptr, false, hasPath))
        {
            const std::string resolvedPathText = resolvedPath.string();
            ImGui::SetClipboardText(resolvedPathText.c_str());
        }

        if (EditorMenuItem("Reveal in File Manager", nullptr, false, resolvedPathExists))
            Platform::RevealPathInFileManager(resolvedPath);

        if (!resolvedPathExists && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
            ImGui::SetTooltip("The source file does not exist on disk.");

        ImGui::Separator();

        const std::string reimportDisabledReason = MakeAssetReimportDisabledReason(record, runtimeModeActive, resolvedPathExists);
        const bool canReimport = reimportDisabledReason.empty();

        if (EditorMenuItem("Reimport", nullptr, false, canReimport))
            pendingReimportAssetID = record.ID;

        if (!canReimport && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
            ImGui::SetTooltip("%s", reimportDisabledReason.c_str());

        const char* removeLabel = selectedAssetCount == 1 ? "Remove from Registry..." : "Remove Selection from Registry...";
        if (EditorMenuItem(removeLabel, nullptr, false, canModifySelection))
        {
            RequestAssetRegistryRemoval(state, state.SelectedAssetIDs);
        }

        const char* deleteLabel = selectedAssetCount == 1 ? "Delete from Project..." : "Delete Selection from Project...";
        if (EditorMenuItem(deleteLabel, "Delete", false, canModifySelection))
        {
            RequestAssetProjectDeletion(state, state.SelectedAssetIDs);
        }

        ImGui::EndPopup();
        ImGui::PopStyleVar(3);
    }

    void DrawAssetFolderContextMenu(
        const char* popupID,
        const Project::ProjectContext& projectContext,
        const std::string& relativeFolderPath,
        const bool runtimeModeActive,
        const AssetBrowserState& state,
        AssetBrowserOperationRequest& operationRequest,
        AssetBrowserFolderActionRequest& folderActionRequest)
    {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, SZ::Px(ImVec2(12.0f, 10.0f)));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, SZ::Px(ImVec2(8.0f, 6.0f)));
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, SZ::Px(ImVec2(8.0f, 5.0f)));

        if (!ImGui::BeginPopupContextItem(popupID))
        {
            ImGui::PopStyleVar(3);
            return;
        }

        DrawFolderContextMenuContents(projectContext, relativeFolderPath, runtimeModeActive, state, operationRequest, folderActionRequest);

        ImGui::EndPopup();
        ImGui::PopStyleVar(3);
    }

    void DrawAssetBrowserBackgroundContextMenu(const char *popupID,
        const Project::ProjectContext &projectContext,
        const std::string &currentAssetFolder,
        bool runtimeModeActive,
        const AssetBrowserState &state,
        AssetBrowserOperationRequest &operationRequest,
        AssetBrowserFolderActionRequest &folderActionRequest)
    {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, SZ::Px(ImVec2(12.0f, 10.0f)));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, SZ::Px(ImVec2(8.0f, 6.0f)));
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, SZ::Px(ImVec2(8.0f, 5.0f)));

        const ImGuiPopupFlags popupFlags = ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems;

        if (!ImGui::BeginPopupContextWindow(popupID, popupFlags))
        {
            ImGui::PopStyleVar(3);
            return;
        }

        DrawFolderContextMenuContents(projectContext, currentAssetFolder, runtimeModeActive, state, operationRequest, folderActionRequest);

        ImGui::EndPopup();
        ImGui::PopStyleVar(3);
    }
}

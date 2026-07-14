#include "Renderer/UI/Assets/ImGuiAssetBrowserContextMenu.h"

#include <filesystem>
#include <string>
#include <imgui.h>

#include "Quiet/Platform/Shell.h"
#include "Renderer/UI/Assets/ImGuiAssetBrowserUtils.h"
#include "Renderer/UI/ImGuiTheme.h"

namespace
{
    namespace SZ = Quiet::Renderer::UI::EditorSizing;

    std::string MakeAssetRemoveDisabledReason(const Quiet::Project::AssetRecord& record, const bool runtimeModeActive, const std::uint32_t referenceCount)
    {
        if (record.BuiltIn)         return "Built-in assets cannot be removed.";
        if (runtimeModeActive)      return "Assets cannot be removed while runtime mode is active.";
        if (referenceCount > 0)     return "This asset is referenced by actors in the current scene.";
        return {};
    }

    std::string MakeAssetReimportDisabledReason(const Quiet::Project::AssetRecord& record, const bool runtimeModeActive, const bool resolvedPathExists)
    {
        if (record.BuiltIn)         return "Built-in assets cannot be reimported.";
        if (runtimeModeActive)      return "Assets cannot be reimported while runtime mode is active.";
        if (record.Path.empty())    return "This asset has no source path.";
        if (!resolvedPathExists)    return "The source file does not exist on disk.";

        return {};
    }

    void RequestFolderAction(Quiet::Renderer::UI::Assets::AssetBrowserFolderActionRequest& request,
        const Quiet::Renderer::UI::Assets::AssetBrowserFolderAction action, const std::string& targetFolder)
    {
        request.Action = action;
        request.TargetFolder = targetFolder;
    }
}

namespace Quiet::Renderer::UI::Assets
{
    void DrawAssetContextMenu(
        const char* popupID,
        const Project::ProjectContext& projectContext,
        const Project::AssetRecord& record,
        const bool runtimeModeActive,
        const std::uint32_t referenceCount,
        std::string& selectedDetailsAssetID,
        std::string& pendingDetailsAssetID,
        std::string& pendingReimportAssetID,
        std::string& pendingRemoveAssetID)
    {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, SZ::Px(ImVec2(12.0f, 10.0f)));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, SZ::Px(ImVec2(8.0f, 6.0f)));
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, SZ::Px(ImVec2(8.0f, 5.0f)));

        if (!ImGui::BeginPopupContextItem(popupID))
        {
            ImGui::PopStyleVar(3);
            return;
        }

        selectedDetailsAssetID = record.ID;

        if (EditorMenuItem("Inspect"))
            pendingDetailsAssetID = record.ID;

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

        const std::string removeDisabledReason = MakeAssetRemoveDisabledReason(record, runtimeModeActive, referenceCount);
        const bool canRemove = removeDisabledReason.empty();

        if (EditorMenuItem("Remove", nullptr, false, canRemove))
            pendingRemoveAssetID = record.ID;

        if (!canRemove && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
            ImGui::SetTooltip("%s", removeDisabledReason.c_str());

        ImGui::EndPopup();
        ImGui::PopStyleVar(3);
    }

    void DrawAssetFolderContextMenu(
        const char* popupID,
        const Project::ProjectContext& projectContext,
        const std::string& relativeFolderPath,
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

        const std::string assetsRoot = GetAssetsRootRelativePath(projectContext);
        const bool isAssetsRoot = relativeFolderPath == assetsRoot;

        if (EditorMenuItem("Create Folder"))
        {
            RequestFolderAction(folderActionRequest, AssetBrowserFolderAction::Create, relativeFolderPath);
        }

        ImGui::Separator();

        if (EditorMenuItem("Rename", nullptr, false, !isAssetsRoot))
        {
            RequestFolderAction(folderActionRequest, AssetBrowserFolderAction::Rename, relativeFolderPath);
        }

        if (EditorMenuItem("Delete Empty Folder", nullptr, false, !isAssetsRoot))
        {
            RequestFolderAction(folderActionRequest, AssetBrowserFolderAction::Delete, relativeFolderPath);
        }

        ImGui::EndPopup();
        ImGui::PopStyleVar(3);
    }
}
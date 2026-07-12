#include "Renderer/UI/Assets/ImGuiAssetBrowserSources.h"

#include <algorithm>
#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

#include <imgui.h>

#include "Renderer/UI/Assets/ImGuiAssetBrowserUtils.h"
#include "Renderer/UI/ImGuiTheme.h"

namespace
{
    namespace QUI = Quiet::Renderer::UI;
    namespace SZ = Quiet::Renderer::UI::EditorSizing;
    namespace Assets = Quiet::Renderer::UI::Assets;

    void RequestFolderAction(Assets::AssetBrowserFolderActionRequest& request, const Assets::AssetBrowserFolderAction action, const std::string& targetFolder)
    {
        request.Action = action;
        request.TargetFolder = targetFolder;
    }

    std::vector<std::filesystem::path> EnumerateChildFolders(const std::filesystem::path& folderPath)
    {
        std::vector<std::filesystem::path> childFolders;
        std::error_code ec;

        if (!std::filesystem::exists(folderPath, ec) || !std::filesystem::is_directory(folderPath, ec))
            return childFolders;

        for (std::filesystem::directory_iterator iterator(folderPath, ec), end; iterator != end; iterator.increment(ec))
        {
            if (ec)
            {
                ec.clear();
                continue;
            }

            std::error_code entryError;

            if (!iterator->is_directory(entryError)) continue;
            const std::string folderName = iterator->path().filename().string();

            if (!folderName.empty() && folderName.front() == '.') continue;
            childFolders.push_back(iterator->path().lexically_normal());
        }

        std::sort(
            childFolders.begin(),
            childFolders.end(),
            [](const std::filesystem::path& left,
               const std::filesystem::path& right)
            {
                return left.generic_string() <
                       right.generic_string();
            }
        );

        return childFolders;
    }

    bool IsFolderOpenForSelection(const std::string& folderPath, const std::string& selectedFolderPath)
    {
        if (folderPath == selectedFolderPath) return true;
        return selectedFolderPath.rfind(folderPath + "/", 0) == 0;
    }

    std::string MakeFolderLabel(const Quiet::Project::ProjectContext& projectContext, const std::filesystem::path& folderPath)
    {
        const std::filesystem::path assetsRoot = projectContext.GetAssetsPath().lexically_normal();

        if (folderPath.lexically_normal() == assetsRoot)
        {
            return Assets::GetAssetsRootRelativePath(projectContext);
        }
        const std::string filename = folderPath.filename().string();
        return filename.empty() ? folderPath.generic_string() : filename;
    }

    void DrawFolderContextMenu(
        const Quiet::Project::ProjectContext& projectContext,
        const std::string& relativeFolderPath,
        Assets::AssetBrowserFolderActionRequest& request)
    {
        const std::string contextMenuID = "##AssetSourceContext_" + relativeFolderPath;
        if (!ImGui::BeginPopupContextItem(contextMenuID.c_str())) return;

        const std::string assetsRoot = Assets::GetAssetsRootRelativePath(projectContext);
        const bool isAssetsRoot = relativeFolderPath == assetsRoot;

        if (QUI::EditorMenuItem("Create Folder"))
            RequestFolderAction(request, Assets::AssetBrowserFolderAction::Create, relativeFolderPath);

        ImGui::Separator();

        if (QUI::EditorMenuItem("Rename", nullptr, false, !isAssetsRoot))
            RequestFolderAction(request, Assets::AssetBrowserFolderAction::Rename, relativeFolderPath);

        if (QUI::EditorMenuItem("Delete Empty Folder", nullptr, false, !isAssetsRoot))
            RequestFolderAction(request, Assets::AssetBrowserFolderAction::Delete, relativeFolderPath);

        ImGui::EndPopup();
    }

    void DrawFolderNode(
        const Quiet::Project::ProjectContext& projectContext,
        const std::filesystem::path& folderPath,
        std::string& currentAssetFolder,
        Assets::AssetBrowserFolderActionRequest& request)
    {
        const std::string relativeFolderPath = Assets::MakeProjectRelativeAssetFolderPath(projectContext, folderPath);
        const std::vector<std::filesystem::path> childFolders = EnumerateChildFolders(folderPath);

        const bool selected = currentAssetFolder == relativeFolderPath;
        const bool hasChildren = !childFolders.empty();

        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
        if (selected) flags |= ImGuiTreeNodeFlags_Selected;
        if (!hasChildren) flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;

        if (IsFolderOpenForSelection(relativeFolderPath, currentAssetFolder))
            ImGui::SetNextItemOpen(true, ImGuiCond_Once);

        const std::string label = MakeFolderLabel(projectContext, folderPath);
        const std::string nodeID = "##AssetSourceFolder_" + relativeFolderPath;

        const bool open = ImGui::TreeNodeEx(nodeID.c_str(), flags, "%s", label.c_str());

        if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) currentAssetFolder = relativeFolderPath;
        if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) currentAssetFolder = relativeFolderPath;

        DrawFolderContextMenu(projectContext, relativeFolderPath, request);

        if (open && hasChildren)
        {
            for (const std::filesystem::path& childFolder : childFolders)
            {
                DrawFolderNode(projectContext, childFolder, currentAssetFolder, request);
            }
            ImGui::TreePop();
        }
    }
}

namespace Quiet::Renderer::UI::Assets
{
    void DrawAssetBrowserSourcesPanel(
        const Project::ProjectContext& projectContext,
        std::string& currentAssetFolder,
        const float width,
        AssetBrowserFolderActionRequest& folderActionRequest)
    {
        ValidateCurrentAssetFolder(projectContext, currentAssetFolder);

        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, SZ::Px(6.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, SZ::Px(ImVec2(10.0f, 10.0f)));

        ImGui::PushStyleColor(ImGuiCol_ChildBg, EditorColor::Background2);
        ImGui::PushStyleColor(ImGuiCol_Border, EditorColor::BorderSoft);

        ImGui::BeginChild("##AssetBrowserSourcesPanel", ImVec2(width, 0.0f), ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding);
        {
            ImGui::AlignTextToFramePadding();

            ImGui::PushStyleColor(ImGuiCol_Text, EditorColor::Text);
            ImGui::TextUnformatted("Sources");
            ImGui::PopStyleColor();

            const float createButtonWidth = SZ::Px(78.0f);
            ImGui::SameLine();

            const float createButtonX = ImGui::GetWindowContentRegionMax().x - createButtonWidth;
            if (createButtonX > ImGui::GetCursorPosX()) ImGui::SetCursorPosX(createButtonX);

            if (EditorButton("+ Folder", ImVec2(createButtonWidth, 0.0f)))
                RequestFolderAction(folderActionRequest, AssetBrowserFolderAction::Create, currentAssetFolder);

            ImGui::PushStyleColor(ImGuiCol_Separator, EditorColor::BorderSoft);
            ImGui::Separator();
            ImGui::PopStyleColor();

            ImGui::Spacing();

            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, SZ::Px(ImVec2(4.0f, 4.0f)));
            ImGui::PushStyleVar(ImGuiStyleVar_IndentSpacing, SZ::Px(18.0f));

            ImGui::PushStyleColor(ImGuiCol_Header, EditorColor::AccentSoft);
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, EditorColor::Background4);
            ImGui::PushStyleColor(ImGuiCol_HeaderActive, EditorColor::AccentDim);

            const std::filesystem::path assetsRootPath = projectContext.GetAssetsPath().lexically_normal();

            std::error_code ec;
            if (!std::filesystem::exists(assetsRootPath, ec) || !std::filesystem::is_directory(assetsRootPath, ec))
            {
                DrawMutedText("Project Assets folder is missing.");
                ImGui::Spacing();

                ImGui::PushStyleColor(ImGuiCol_Text, EditorColor::TextDim);
                ImGui::TextWrapped("%s", assetsRootPath.generic_string().c_str());
                ImGui::PopStyleColor();
            }
            else
            {
                DrawFolderNode(projectContext, assetsRootPath, currentAssetFolder, folderActionRequest);
            }

            ImGui::PopStyleColor(3);
            ImGui::PopStyleVar(2);
        }
        ImGui::EndChild();

        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(2);
    }
}
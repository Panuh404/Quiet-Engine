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

    std::vector<std::filesystem::path> EnumerateChildFolders(const std::filesystem::path& folderPath)
    {
        std::vector<std::filesystem::path> childFolders;

        std::error_code ec;
        if (!std::filesystem::exists(folderPath, ec) || !std::filesystem::is_directory(folderPath, ec))
            return childFolders;

        for (std::filesystem::directory_iterator it(folderPath, ec), end; it != end; it.increment(ec))
        {
            if (ec)
            {
                ec.clear();
                continue;
            }

            std::error_code entryError;
            if (!it->is_directory(entryError))
                continue;

            const std::string folderName = it->path().filename().string();
            if (!folderName.empty() && folderName.front() == '.')
                continue;

            childFolders.push_back(it->path().lexically_normal());
        }

        std::sort(
            childFolders.begin(),
            childFolders.end(),
            [](const std::filesystem::path& left, const std::filesystem::path& right)
            {
                return left.generic_string() < right.generic_string();
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
            return Assets::GetAssetsRootRelativePath(projectContext);

        const std::string filename = folderPath.filename().string();
        return filename.empty() ? folderPath.generic_string() : filename;
    }

    void DrawFolderNode(const Quiet::Project::ProjectContext& projectContext, const std::filesystem::path& folderPath, std::string& currentAssetFolder)
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

        if (ImGui::IsItemHovered())
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

        if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
            currentAssetFolder = relativeFolderPath;

        if (open && hasChildren)
        {
            for (const std::filesystem::path& childFolder : childFolders)
                DrawFolderNode(projectContext, childFolder, currentAssetFolder);

            ImGui::TreePop();
        }
    }
}

namespace Quiet::Renderer::UI::Assets
{
    void DrawAssetBrowserSourcesPanel(const Project::ProjectContext& projectContext, std::string& currentAssetFolder, const float width)
    {
        ValidateCurrentAssetFolder(projectContext, currentAssetFolder);

        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, SZ::Px(10.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, SZ::Px(ImVec2(12.0f, 12.0f)));

        ImGui::PushStyleColor(ImGuiCol_ChildBg, EditorColor::Background2);
        ImGui::PushStyleColor(ImGuiCol_Border, EditorColor::BorderSoft);

        ImGui::BeginChild("##AssetBrowserSourcesPanel", ImVec2(width, 0.0f), ImGuiChildFlags_Borders);
        {
            DrawSectionTitle("Sources");
            ImGui::Spacing();

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
                DrawFolderNode(projectContext, assetsRootPath, currentAssetFolder);
            }

            ImGui::Spacing();

            ImGui::PushStyleColor(ImGuiCol_Separator, EditorColor::BorderSoft);
            ImGui::Separator();
            ImGui::PopStyleColor();

            DrawMutedText("Selected Folder");

            ImGui::PushStyleColor(ImGuiCol_Text, EditorColor::Text);
            ImGui::TextWrapped("%s", currentAssetFolder.c_str());
            ImGui::PopStyleColor();
        }
        ImGui::EndChild();

        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(2);
    }
}
#include "Renderer/UI/Assets/ImGuiAssetBrowserSources.h"

#include <algorithm>
#include <filesystem>
#include <string>
#include <system_error>
#include <utility>
#include <vector>
#include <imgui.h>

#include "Renderer/UI/Assets/ImGuiAssetBrowserUtils.h"
#include "Renderer/UI/Assets/ImGuiAssetBrowserWidgets.h"
#include "Renderer/UI/Assets/ImGuiAssetBrowserContextMenu.h"
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

    bool IsFolderOpenForSelection(const std::string& folderPath, const std::string& selectedFolderPath)
    {
        if (folderPath == selectedFolderPath) return true;
        return selectedFolderPath.rfind(folderPath + "/", 0) == 0;
    }

    void ActivateProjectFolder(Assets::AssetBrowserState& state, std::string& currentAssetFolder, const std::string& relativeFolderPath)
    {
        Assets::SetAssetBrowserSource(state, Assets::AssetBrowserSource::Project);
        currentAssetFolder = relativeFolderPath;
    }

    void ActivateBuiltInSource(Assets::AssetBrowserState& state)
    {
        Assets::SetAssetBrowserSource(state, Assets::AssetBrowserSource::EngineBuiltIn);
    }

    void DrawFolderNode(
        const Quiet::Project::ProjectContext& projectContext,
        const std::string& relativeFolderPath,
        std::string& currentAssetFolder,
        const bool runtimeModeActive,
        Assets::AssetBrowserState& state,
        Assets::AssetBrowserFolderActionRequest& folderActionRequest,
        Assets::AssetBrowserAssetMoveRequest& assetMoveRequest,
        Assets::AssetBrowserOperationRequest& operationRequest)
    {
        const std::vector<Assets::AssetBrowserFolderEntry>childFolders = Assets::BuildAssetChildFolderList(projectContext, relativeFolderPath);

        const bool selected = state.CurrentSource == Assets::AssetBrowserSource::Project && currentAssetFolder == relativeFolderPath;
        const bool hasChildren = !childFolders.empty();

        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;

        if (selected)
            flags |= ImGuiTreeNodeFlags_Selected;

        if (!hasChildren)
            flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;

        if (state.CurrentSource == Assets::AssetBrowserSource::Project && IsFolderOpenForSelection(relativeFolderPath, currentAssetFolder))
            ImGui::SetNextItemOpen(true, ImGuiCond_Once);

        const std::string assetsRoot = Assets::GetAssetsRootRelativePath(projectContext);
        const std::string label = relativeFolderPath == assetsRoot ? assetsRoot : std::filesystem::path(relativeFolderPath).filename().string();
        const std::string nodeID = "##AssetSourceFolder_" + relativeFolderPath;
        const bool open = ImGui::TreeNodeEx(nodeID.c_str(), flags, "%s", label.c_str());

        if (ImGui::IsItemHovered())
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

        if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
            ActivateProjectFolder(state, currentAssetFolder, relativeFolderPath);

        if (Assets::IsAssetBrowserContextMenuRequestedForLastItem())
            ActivateProjectFolder(state, currentAssetFolder,relativeFolderPath);

        std::vector<std::string> droppedAssetIDs;
        if (Assets::AcceptAssetBrowserDrop(droppedAssetIDs))
        {
            assetMoveRequest.AssetIDs = std::move(droppedAssetIDs);
            assetMoveRequest.TargetFolder = relativeFolderPath;
        }

        const std::string contextMenuID = "##AssetSourceContext_" + relativeFolderPath;

        Assets::DrawAssetFolderContextMenu(
            contextMenuID.c_str(),
            projectContext,
            relativeFolderPath,
            runtimeModeActive,
            state,
            operationRequest,
            folderActionRequest);

        if (open && hasChildren)
        {
            for (const Assets::AssetBrowserFolderEntry& child : childFolders)
            {
                DrawFolderNode(
                    projectContext,
                    child.RelativePath,
                    currentAssetFolder,
                    runtimeModeActive,
                    state,
                    folderActionRequest,
                    assetMoveRequest,
                    operationRequest);
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
        float width,
        bool runtimeModeActive,
        AssetBrowserState& state,
        AssetBrowserFolderActionRequest& folderActionRequest,
        AssetBrowserAssetMoveRequest& assetMoveRequest,
        AssetBrowserOperationRequest& operationRequest)
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

            const bool canCreateFolder = !runtimeModeActive && state.CurrentSource == AssetBrowserSource::Project;

            if (EditorButton("+ Folder", ImVec2(createButtonWidth, 0.0f), canCreateFolder))
                RequestFolderAction(folderActionRequest, AssetBrowserFolderAction::Create,currentAssetFolder);

            if (!canCreateFolder && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
            {
                ImGui::SetTooltip(state.CurrentSource == AssetBrowserSource::EngineBuiltIn
                    ? "The Engine source is read-only."
                    : "Folders cannot be created during runtime mode.");
            }

            ImGui::PushStyleColor(ImGuiCol_Separator, EditorColor::BorderSoft);
            ImGui::Separator();
            ImGui::PopStyleColor();

            ImGui::Spacing();

            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, SZ::Px(ImVec2(4.0f, 4.0f)));
            ImGui::PushStyleVar(ImGuiStyleVar_IndentSpacing, SZ::Px(18.0f));

            ImGui::PushStyleColor(ImGuiCol_Header, EditorColor::AccentSoft);
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, EditorColor::Background4);
            ImGui::PushStyleColor(ImGuiCol_HeaderActive, EditorColor::AccentDim);

            const ImGuiTreeNodeFlags groupFlags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
            ImGui::SetNextItemOpen(true, ImGuiCond_Once);

            const bool projectGroupOpen = ImGui::TreeNodeEx("##AssetSourceProject", groupFlags, "Project");
            if (projectGroupOpen)
            {
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
                    DrawFolderNode(
                        projectContext,
                        GetAssetsRootRelativePath(projectContext),
                        currentAssetFolder,
                        runtimeModeActive,
                        state,
                        folderActionRequest,
                        assetMoveRequest,
                        operationRequest
                    );
                }

                ImGui::TreePop();
            }

            ImGui::SetNextItemOpen(
                true,
                ImGuiCond_Once
            );

            const bool engineGroupOpen =
                ImGui::TreeNodeEx(
                    "##AssetSourceEngine",
                    groupFlags,
                    "Engine"
                );

            if (engineGroupOpen)
            {
                ImGuiTreeNodeFlags builtInFlags =
                    ImGuiTreeNodeFlags_Leaf |
                    ImGuiTreeNodeFlags_NoTreePushOnOpen |
                    ImGuiTreeNodeFlags_SpanAvailWidth;

                const bool builtInSelected =
                    state.CurrentSource ==
                    AssetBrowserSource::EngineBuiltIn;

                if (builtInSelected)
                {
                    builtInFlags |=
                        ImGuiTreeNodeFlags_Selected;
                }

                ImGui::TreeNodeEx(
                    "##AssetSourceBuiltIn",
                    builtInFlags,
                    "Built-In"
                );

                if (ImGui::IsItemHovered())
                {
                    ImGui::SetMouseCursor(
                        ImGuiMouseCursor_Hand
                    );
                }

                if (ImGui::IsItemClicked(
                        ImGuiMouseButton_Left))
                {
                    ActivateBuiltInSource(
                        state
                    );
                }

                ImGui::TreePop();
            }

            ImGui::PopStyleColor(3);
            ImGui::PopStyleVar(2);
        }
        ImGui::EndChild();

        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(2);
    }
}
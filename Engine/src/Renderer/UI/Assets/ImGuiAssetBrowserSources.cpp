#include "Renderer/UI/Assets/ImGuiAssetBrowserSources.h"

#include <algorithm>
#include <filesystem>
#include <string>
#include <system_error>
#include <utility>
#include <vector>
#include <imgui.h>

#include "Renderer/UI/ImGuiToolbarCommon.h"
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

    void ActivateEngineSource(Assets::AssetBrowserState& state)
    {
        Assets::SetAssetBrowserSource(state, Assets::AssetBrowserSource::EngineBuiltIn);
    }

    void DrawSourceSectionHeader(const char* label)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::TextDim);
        ImGui::TextUnformatted(label);
        ImGui::PopStyleColor();

        const ImVec2 labelMin = ImGui::GetItemRectMin();
        const ImVec2 labelMax = ImGui::GetItemRectMax();

        const float lineStartX = labelMax.x + SZ::Px(8.0f);
        const float lineEndX = ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x;

        if (lineEndX > lineStartX)
        {
            const float lineY = labelMin.y + (labelMax.y - labelMin.y) * 0.5f;
            ImGui::GetWindowDrawList()->AddLine(
                ImVec2(lineStartX, lineY),
                ImVec2(lineEndX, lineY),
                ImGui::ColorConvertFloat4ToU32(QUI::EditorColor::BorderSoft),
                SZ::Px(1.0f)
            );
        }

        ImGui::Dummy(ImVec2(0.0f, SZ::Px(3.0f)));
    }

    void DrawFolderNode(const Quiet::Project::ProjectContext& projectContext,
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
        const std::string label = relativeFolderPath == assetsRoot ? "Project" : std::filesystem::path(relativeFolderPath).filename().string();
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
    void DrawAssetBrowserSourcesPanel(const Project::ProjectContext& projectContext,
                                      std::string& currentAssetFolder,
                                      float width,
                                      bool runtimeModeActive,
                                      const EditorFonts& fonts,
                                      AssetBrowserState& state,
                                      AssetBrowserFolderActionRequest& folderActionRequest,
                                      AssetBrowserAssetMoveRequest& assetMoveRequest,
                                      AssetBrowserOperationRequest& operationRequest)
    {
        ValidateCurrentAssetFolder(projectContext, currentAssetFolder);

        const float collapsedWidth = SZ::Px(42.0f);
        const float panelWidth = state.SourcesPanelExpanded ? width : collapsedWidth;

        const ImVec2 panelPadding = state.SourcesPanelExpanded ? SZ::Px(ImVec2(10.0f, 10.0f)) : SZ::Px(ImVec2(7.0f, 10.0f));

        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, SZ::Px(6.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, panelPadding);

        ImGui::PushStyleColor(ImGuiCol_ChildBg, EditorColor::Background2);
        ImGui::PushStyleColor(ImGuiCol_Border, EditorColor::BorderSoft);

        ImGui::BeginChild("##AssetBrowserSourcesPanel", ImVec2(panelWidth, 0.0f), ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding);
        {
            if (!state.SourcesPanelExpanded)
            {
                if (DrawToolbarButton(fonts, "ExpandAssetSources", Icons::ChevronRight, ">", "Show Sources", false, true, SZ::Px(ImVec2(28.0f, 28.0f))))
                {
                    state.SourcesPanelExpanded = true;
                }
            }
            else
            {
                ImGui::AlignTextToFramePadding();

                ImGui::PushStyleColor(ImGuiCol_Text, EditorColor::Text);
                ImGui::TextUnformatted("Sources");
                ImGui::PopStyleColor();

                const float collapseButtonWidth = SZ::Px(28.0f);
                const float createButtonWidth = fonts.HasIcons ? SZ::Px(30.0f) : SZ::Px(78.0f);
                const float controlsWidth = collapseButtonWidth + ImGui::GetStyle().ItemSpacing.x + createButtonWidth;

                ImGui::SameLine();

                const float controlsX = ImGui::GetWindowContentRegionMax().x - controlsWidth;
                if (controlsX > ImGui::GetCursorPosX())
                {
                    ImGui::SetCursorPosX(controlsX);
                }

                if (DrawToolbarButton(fonts, "CollapseAssetSources", Icons::ChevronLeft, "<", "Collapse Sources", false, true, ImVec2(collapseButtonWidth, 0.0f)))
                {
                    state.SourcesPanelExpanded = false;
                }

                ImGui::SameLine();

                const bool canCreateFolder = !runtimeModeActive && state.CurrentSource == AssetBrowserSource::Project;
                if (DrawToolbarButton(fonts, "CreateAssetFolder", Icons::FolderPlus, "+ Folder", "Create Folder", false, canCreateFolder, ImVec2(createButtonWidth, 0.0f)))
                {
                    RequestFolderAction(folderActionRequest, AssetBrowserFolderAction::Create, currentAssetFolder);
                }

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

                const std::string assetsRoot = GetAssetsRootRelativePath(projectContext);
                const std::filesystem::path assetsRootPath = projectContext.GetAssetsPath().lexically_normal();

                std::error_code ec;

                const bool projectAssetsAvailable = std::filesystem::exists(assetsRootPath, ec) && !ec &&
                    std::filesystem::is_directory(assetsRootPath, ec) && !ec;

                DrawSourceSectionHeader("PROJECT CONTENT");

                if (projectAssetsAvailable)
                {
                    // The physical Assets root is represented directly by the
                    // top-level Project source. Its child folders appear beneath it.
                    DrawFolderNode(
                        projectContext,
                        assetsRoot,
                        currentAssetFolder,
                        runtimeModeActive,
                        state,
                        folderActionRequest,
                        assetMoveRequest,
                        operationRequest
                    );
                }
                else
                {
                    ImGuiTreeNodeFlags projectFlags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_SpanAvailWidth;
                    if (state.CurrentSource == AssetBrowserSource::Project)
                    {
                        projectFlags |= ImGuiTreeNodeFlags_Selected;
                    }

                    ImGui::TreeNodeEx("##AssetSourceProjectUnavailable", projectFlags, "Project");

                    if (ImGui::IsItemHovered())
                    {
                        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                    }

                    if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
                    {
                        ActivateProjectFolder(state, currentAssetFolder,assetsRoot);
                    }

                    ImGui::Indent(SZ::Px(18.0f));

                    DrawMutedText("Project Assets folder is missing.");

                    ImGui::PushStyleColor(ImGuiCol_Text, EditorColor::TextDim);
                    ImGui::TextWrapped("%s", assetsRootPath.generic_string().c_str());
                    ImGui::PopStyleColor();

                    ImGui::Unindent(SZ::Px(18.0f));
                }

                ImGui::Dummy(ImVec2(0.0f, SZ::Px(10.0f)));
                DrawSourceSectionHeader("ENGINE CONTENT");

                ImGuiTreeNodeFlags engineFlags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_SpanAvailWidth;
                if (state.CurrentSource == AssetBrowserSource::EngineBuiltIn)
                {
                    engineFlags |= ImGuiTreeNodeFlags_Selected;
                }

                ImGui::TreeNodeEx("##AssetSourceEngine", engineFlags, "Engine");

                if (ImGui::IsItemHovered())
                {
                    ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                    ImGui::SetTooltip("Read-only built-in engine assets.");
                }

                if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
                {
                    ActivateEngineSource(state);
                }

                ImGui::PopStyleColor(3);
                ImGui::PopStyleVar(2);
            }
            ImGui::EndChild();

            ImGui::PopStyleColor(2);
            ImGui::PopStyleVar(2);
        }
    }
}
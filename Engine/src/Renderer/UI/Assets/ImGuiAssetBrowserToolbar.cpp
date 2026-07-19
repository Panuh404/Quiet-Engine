#include "Renderer/UI/Assets/ImGuiAssetBrowserToolbar.h"

#include <cstddef>
#include <string>

#include <imgui.h>
#include <imgui_internal.h>

#include "ImGuiAssetBrowserDialogs.h"
#include "Renderer/UI/Assets/ImGuiAssetBrowserOperations.h"
#include "Renderer/UI/Assets/ImGuiAssetBrowserUtils.h"
#include "Renderer/UI/ImGuiTheme.h"

namespace
{
    namespace QUI = Quiet::Renderer::UI;
    namespace SZ = Quiet::Renderer::UI::EditorSizing;

    std::string MakeAssetTypeFilterDropdownLabel(
         const Quiet::Renderer::UI::Assets::AssetBrowserTypeFilter filter,
         const std::size_t meshCount,
         const std::size_t textureCount)
    {
        using Quiet::Renderer::UI::Assets::AssetBrowserTypeFilter;
        switch (filter)
        {
            case AssetBrowserTypeFilter::Meshes:    return "Filter: Meshes " + std::to_string(meshCount);
            case AssetBrowserTypeFilter::Textures:  return "Filter: Textures " + std::to_string(textureCount);
            case AssetBrowserTypeFilter::All:
            default:                                return "Filter: All";
        }
    }

    const char* ToAssetBrowserViewModeLabel(const Quiet::Renderer::UI::Assets::AssetBrowserViewMode viewMode)
    {
        using Quiet::Renderer::UI::Assets::AssetBrowserViewMode;
        switch (viewMode)
        {
            case AssetBrowserViewMode::Browser: return "Browser";
            case AssetBrowserViewMode::List:
            default:                            return "List";
        }
    }

    const char* ToAssetBrowserThumbnailSizeLabel(const Quiet::Renderer::UI::Assets::AssetBrowserThumbnailSize thumbnailSize)
    {
        using Quiet::Renderer::UI::Assets::AssetBrowserThumbnailSize;
        switch (thumbnailSize)
        {
            case AssetBrowserThumbnailSize::Small:  return "Small";
            case AssetBrowserThumbnailSize::Large:  return "Large";
            case AssetBrowserThumbnailSize::Medium:
            default:                                return "Medium";
        }
    }

    bool DrawAssetToolbarDropdownButton(const std::string& label, const char* popupID, const ImVec2& size)
    {
        if (QUI::EditorButton(label.c_str(), size))
            ImGui::OpenPopup(popupID);

        return ImGui::BeginPopup(popupID);
    }

    std::string MakeDiagnosticsDropdownLabel(
        const Quiet::Renderer::UI::Assets::AssetBrowserDiagnosticsFilter filter,
        const std::size_t issueCount,
        const std::size_t missingCount,
        const std::size_t externalCount)
    {
        using Quiet::Renderer::UI::Assets::AssetBrowserDiagnosticsFilter;

        switch (filter)
        {
            case AssetBrowserDiagnosticsFilter::Issues:
                return "Diagnostics: Issues " + std::to_string(issueCount);

            case AssetBrowserDiagnosticsFilter::Missing:
                return "Diagnostics: Missing " + std::to_string(missingCount);

            case AssetBrowserDiagnosticsFilter::External:
                return "Diagnostics: External " + std::to_string(externalCount);

            case AssetBrowserDiagnosticsFilter::All:
            default:
                return "Diagnostics: All";
        }
    }

}

namespace Quiet::Renderer::UI::Assets
{
    void DrawAssetBrowserToolbar(
        Editor::EditorState& editorState,
        const Project::ProjectContext& projectContext,
        std::array<char, 128>& searchBuffer,
        AssetBrowserTypeFilter& typeFilter,
        AssetBrowserDiagnosticsFilter& diagnosticsFilter,
        AssetBrowserViewMode& viewMode,
        AssetBrowserThumbnailSize& thumbnailSize,
        AssetBrowserState& state,
        AssetBrowserOperationRequest& operationRequest,
        bool& openImportPopup)
    {
        const std::size_t selectedAssetCount = state.SelectedAssetIDs.size();
        const bool clipboardAvailable = IsAssetClipboardAvailable(projectContext, state);
        const Project::AssetRegistry& registry = projectContext.GetAssetRegistry();
        const std::size_t meshCount = CountAssetByType(registry, Project::AssetType::Mesh);
        const std::size_t textureCount = CountAssetByType(registry, Project::AssetType::Texture);
        const AssetDiagnosticsCounts diagnosticsCounts = CountAssetDiagnostics(projectContext);

        const std::size_t issueCount = diagnosticsCounts.Issues;
        const std::size_t missingCount = diagnosticsCounts.Missing;
        const std::size_t externalCount = diagnosticsCounts.External;

        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, SZ::Px(ImVec2(10.0f, 7.0f)));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, SZ::Px(ImVec2(8.0f, 8.0f)));

        if (EditorButton("Import", SZ::Px(ImVec2(86.0f, 0.0f))))
            openImportPopup = true;

        ImGui::SameLine();

        if (EditorButton("Reload", SZ::Px(ImVec2(86.0f, 0.0f))))
            editorState.QueueRequest(Editor::EditorRequestType::ReloadAssets);

        if (selectedAssetCount > 0)
        {
            ImGui::SameLine();

            ImGui::PushStyleColor(ImGuiCol_Text, EditorColor::Accent);
            ImGui::Text("%zu selected", selectedAssetCount);
            ImGui::PopStyleColor();
        }

        if (selectedAssetCount > 0 || clipboardAvailable)
        {
            ImGui::SameLine();

            if (DrawAssetToolbarDropdownButton("Actions", "##AssetSelectionActions", SZ::Px(ImVec2(104.0f, 0.0f))))
            {
                const bool editMode = editorState.Scene.Mode == Editor::SceneMode::Edit;

                if (selectedAssetCount > 0)
                {
                    // COPY
                    const char* copyLabel = selectedAssetCount == 1 ? "Copy Asset" : "Copy Selection";
                    if (EditorMenuItem(copyLabel, "Ctrl+C", false, editMode))
                    {
                        operationRequest.Clear();
                        operationRequest.Action = AssetBrowserOperationAction::CopySelection;
                        operationRequest.AssetIDs = state.SelectedAssetIDs;
                    }

                    // CUT
                    const char* cutLabel = selectedAssetCount == 1 ? "Cut Asset" : "Cut Selection";
                    if (EditorMenuItem(cutLabel, "Ctrl+X", false, editMode))
                    {
                        operationRequest.Clear();
                        operationRequest.Action = AssetBrowserOperationAction::CutSelection;
                        operationRequest.AssetIDs = state.SelectedAssetIDs;
                    }

                    // RENAME
                    const Project::AssetRecord* renameRecord = selectedAssetCount == 1
                        ? Project::AssetRegistryIO::FindByID(projectContext.GetAssetRegistry(), state.SelectedAssetIDs.front())
                        : nullptr;

                    bool canRename = editMode && renameRecord && !renameRecord->BuiltIn && !renameRecord->Path.empty();
                    if (canRename)
                    {
                        const AssetPathDiagnostics diagnostics = InspectAssetPath(projectContext, *renameRecord);
                        canRename = !diagnostics.IsMissing() && !diagnostics.External;
                    }

                    if (EditorMenuItem("Rename", "F2", false, canRename))
                    {
                        RequestAssetRename(state, *renameRecord);
                    }

                    // DUPLICATE
                    const char* duplicateLabel = selectedAssetCount == 1 ? "Duplicate" : "Duplicate Selection";
                    if (EditorMenuItem(duplicateLabel, "Ctrl+D", false, editMode))
                    {
                        operationRequest.Clear();
                        operationRequest.Action = AssetBrowserOperationAction::Duplicate;
                        operationRequest.AssetIDs = state.SelectedAssetIDs;
                        operationRequest.TargetFolder = state.CurrentAssetFolder;
                    }
                }

                if (selectedAssetCount > 0 && clipboardAvailable)
                {
                    ImGui::Separator();
                }

                if (clipboardAvailable)
                {
                    const bool copying = state.Clipboard.Mode == AssetBrowserClipboardMode::Copy;
                    const std::string pasteLabel = copying
                        ? (state.Clipboard.AssetIDs.size() == 1 ? "Paste Copy" : "Paste Copies")
                        : (state.Clipboard.AssetIDs.size() == 1 ? "Move Here" : "Move Selection Here");

                    if (EditorMenuItem(pasteLabel.c_str(), "Ctrl+V", false, editMode))
                    {
                        operationRequest.Clear();
                        operationRequest.Action = AssetBrowserOperationAction::Paste;
                        operationRequest.TargetFolder = state.CurrentAssetFolder;
                    }
                }

                ImGui::EndPopup();
            }
        }

        ImGui::SameLine();

        ImGui::SetNextItemWidth(SZ::Px(260.0f));
        ImGui::InputTextWithHint("##AssetSearch", "Search assets...", searchBuffer.data(), searchBuffer.size());

        if (searchBuffer[0] != '\0')
        {
            ImGui::SameLine();
            if (EditorButton("Clear", SZ::Px(ImVec2(72.0f, 0.0f))))
                searchBuffer[0] = '\0';
        }

        ImGui::SameLine();
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
        ImGui::SameLine();

        const std::size_t totalAssetCount = registry.Records.size();
        const std::string filterDropdownLabel = MakeAssetTypeFilterDropdownLabel(typeFilter, meshCount, textureCount);
        if (DrawAssetToolbarDropdownButton(filterDropdownLabel, "##AssetTypeFilterDropdown", SZ::Px(ImVec2(166.0f, 0.0f))))
        {
            const std::string allLabel = "All " + std::to_string(totalAssetCount);
            const std::string meshesLabel = "Meshes " + std::to_string(meshCount);
            const std::string texturesLabel = "Textures " + std::to_string(textureCount);

            // All
            if (EditorMenuItem(allLabel.c_str(), nullptr, typeFilter == AssetBrowserTypeFilter::All))
                typeFilter = AssetBrowserTypeFilter::All;

            // Meshes
            if (EditorMenuItem(meshesLabel.c_str(), nullptr, typeFilter == AssetBrowserTypeFilter::Meshes))
                typeFilter = AssetBrowserTypeFilter::Meshes;

            // Textures
            if (EditorMenuItem(texturesLabel.c_str(), nullptr, typeFilter == AssetBrowserTypeFilter::Textures))
                typeFilter = AssetBrowserTypeFilter::Textures;

            ImGui::EndPopup();
        }

        ImGui::SameLine();

        const std::string diagnosticsDropdownLabel = MakeDiagnosticsDropdownLabel(diagnosticsFilter, issueCount, missingCount, externalCount);
        if (DrawAssetToolbarDropdownButton(diagnosticsDropdownLabel, "##AssetDiagnosticsDropdown", SZ::Px(ImVec2(190.0f, 0.0f))))
        {
            // All
            if (EditorMenuItem("All", nullptr, diagnosticsFilter == AssetBrowserDiagnosticsFilter::All))
                diagnosticsFilter = AssetBrowserDiagnosticsFilter::All;

            // Issues
            const std::string issuesLabel = "Issues " + std::to_string(issueCount);
            if (EditorMenuItem(issuesLabel.c_str(), nullptr, diagnosticsFilter == AssetBrowserDiagnosticsFilter::Issues))
                diagnosticsFilter = AssetBrowserDiagnosticsFilter::Issues;

            // Missing
            const std::string missingLabel = "Missing " + std::to_string(missingCount);
            if (EditorMenuItem(missingLabel.c_str(), nullptr, diagnosticsFilter == AssetBrowserDiagnosticsFilter::Missing))
                diagnosticsFilter = AssetBrowserDiagnosticsFilter::Missing;

            // External
            const std::string externalLabel = "External " + std::to_string(externalCount);
            if (EditorMenuItem(externalLabel.c_str(), nullptr, diagnosticsFilter == AssetBrowserDiagnosticsFilter::External))
                diagnosticsFilter = AssetBrowserDiagnosticsFilter::External;

            ImGui::EndPopup();
        }

        ImGui::SameLine();

        const std::string viewDropdownLabel = std::string("View: ") + ToAssetBrowserViewModeLabel(viewMode);

        if (DrawAssetToolbarDropdownButton(viewDropdownLabel, "##AssetViewModeDropdown", SZ::Px(ImVec2(142.0f, 0.0f))))
        {
            // List
            if (EditorMenuItem("List", nullptr, viewMode == AssetBrowserViewMode::List))
                viewMode = AssetBrowserViewMode::List;

            // Browser
            if (EditorMenuItem("Browser", nullptr, viewMode == AssetBrowserViewMode::Browser))
                viewMode = AssetBrowserViewMode::Browser;

            ImGui::EndPopup();
        }

        if (viewMode == AssetBrowserViewMode::Browser)
        {
            ImGui::SameLine();
            const std::string sizeDropdownLabel = std::string("Size: ") + ToAssetBrowserThumbnailSizeLabel(thumbnailSize);

            if (DrawAssetToolbarDropdownButton(sizeDropdownLabel, "##AssetThumbnailSizeDropdown", SZ::Px(ImVec2(142.0f, 0.0f))))
            {
                // Small
                if (EditorMenuItem("Small", nullptr, thumbnailSize == AssetBrowserThumbnailSize::Small))
                    thumbnailSize = AssetBrowserThumbnailSize::Small;

                // Medium
                if (EditorMenuItem("Medium", nullptr, thumbnailSize == AssetBrowserThumbnailSize::Medium))
                    thumbnailSize = AssetBrowserThumbnailSize::Medium;

                // Large
                if (EditorMenuItem("Large", nullptr, thumbnailSize == AssetBrowserThumbnailSize::Large))
                    thumbnailSize = AssetBrowserThumbnailSize::Large;

                ImGui::EndPopup();
            }
        }

        ImGui::PopStyleVar(2);
    }
}

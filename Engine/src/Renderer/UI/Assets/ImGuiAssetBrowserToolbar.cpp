#include "Renderer/UI/Assets/ImGuiAssetBrowserToolbar.h"

#include <cstddef>
#include <string>
#include <algorithm>

#include <imgui.h>
#include <imgui_internal.h>

#include "Quiet/Project/AssetRegistry.h"
#include "Renderer/Vulkan/Mesh/MeshLibrary.h"
#include "Renderer/Vulkan/Mesh/MeshPrimitives.h"
#include "Renderer/UI/ImGuiToolbarCommon.h"
#include "Renderer/UI/Assets/ImGuiAssetBrowserOperations.h"
#include "Renderer/UI/Assets/ImGuiAssetBrowserUtils.h"
#include "Renderer/UI/Assets/ImGuiAssetBrowserDialogs.h"
#include "Renderer/UI/ImGuiTheme.h"

namespace
{
    namespace QUI = Quiet::Renderer::UI;
    namespace SZ = Quiet::Renderer::UI::EditorSizing;
    namespace Assets = Quiet::Renderer::UI::Assets;
    namespace Editor = Quiet::Editor;
    namespace Project = Quiet::Project;
    namespace Vulkan = Quiet::Renderer::Vulkan;

    std::string MakeAssetTypeFilterDropdownLabel(const Assets::AssetBrowserTypeFilter filter,
                                                 const std::size_t meshCount,
                                                 const std::size_t textureCount,
                                                 const std::size_t audioCount,
                                                 const std::size_t scriptCount)
    {
        using Assets::AssetBrowserTypeFilter;
        switch (filter)
        {
            case AssetBrowserTypeFilter::Meshes:    return "Filter: Meshes "    + std::to_string(meshCount);
            case AssetBrowserTypeFilter::Textures:  return "Filter: Textures "  + std::to_string(textureCount);
            case AssetBrowserTypeFilter::Audio:     return "Filter: Audio "     + std::to_string(audioCount);
            case AssetBrowserTypeFilter::Scripts:   return "Filter: Scripts "   + std::to_string(scriptCount);
            case AssetBrowserTypeFilter::All:
            default:                                return "Filter: All";
        }
    }

    const char* ToAssetBrowserViewModeLabel(const Assets::AssetBrowserViewMode viewMode)
    {
        using Assets::AssetBrowserViewMode;
        switch (viewMode)
        {
            case AssetBrowserViewMode::Browser: return "Browser";
            case AssetBrowserViewMode::List:
            default:                            return "List";
        }
    }

    const char* ToAssetBrowserThumbnailSizeLabel(const Assets::AssetBrowserThumbnailSize thumbnailSize)
    {
        using Assets:: AssetBrowserThumbnailSize;
        switch (thumbnailSize)
        {
            case AssetBrowserThumbnailSize::Tiny:       return "Tiny";
            case AssetBrowserThumbnailSize::Small:      return "Small";
            case AssetBrowserThumbnailSize::Large:      return "Large";
            case AssetBrowserThumbnailSize::ExtraLarge: return "Extra Large";
            case AssetBrowserThumbnailSize::Medium:
            default:                                    return "Medium";
        }
    }

    bool DrawAssetToolbarDropdownButton(const QUI::EditorFonts& fonts,
                                        const char* id,
                                        const char* icon,
                                        const char* fallback,
                                        const char* tooltip,
                                        const char* popupID,
                                        const bool active,
                                        const ImVec2& size)
    {
        if (QUI::DrawToolbarButton(fonts, id, icon, fallback, tooltip, active, true, size))
        {
            ImGui::OpenPopup(popupID);
        }

        return ImGui::BeginPopup(popupID);
    }

    std::string MakeDiagnosticsDropdownLabel(const Assets::AssetBrowserDiagnosticsFilter filter,
                                             const std::size_t issueCount,
                                             const std::size_t missingCount,
                                             const std::size_t externalCount)
    {
        using Assets::AssetBrowserDiagnosticsFilter;

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

    void DrawPrimitiveCreationPopup(Editor::EditorState& editorState,
                                    const Project::ProjectContext& projectContext,
                                    const Vulkan::MeshLibrary& meshLibrary)
    {
        if (!ImGui::BeginPopup("##AssetBrowserCreatePrimitive")) return;
        QUI::DrawMutedText("Create a primitive actor at the world origin.");

        ImGui::Separator();

        const bool editMode = editorState.Scene.Mode == Editor::SceneMode::Edit;
        for (const std::string& primitiveID : Vulkan::MeshPrimitives::GetBuiltInAssetIDs())
        {
            const Project::AssetRecord* record = Project::AssetRegistryIO::FindByID(projectContext.GetAssetRegistry(), primitiveID);
            const bool registered = record && record->BuiltIn && record->Type == Project::AssetType::Mesh;
            const bool loaded = registered && meshLibrary.Has(primitiveID);
            const bool canCreate = editMode && loaded;

            if (QUI::EditorMenuItem(primitiveID.c_str(), nullptr, false, canCreate))
            {
                editorState.QueueRequest(Editor::EditorRequestType::CreatePrimitiveActor, primitiveID);
                ImGui::CloseCurrentPopup();
            }

            if (!canCreate && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
            {
                if (!editMode)
                {
                    ImGui::SetTooltip("Primitive actors can only be created in Edit mode.");
                }
                else if (!registered)
                {
                    ImGui::SetTooltip("Primitive '%s' is not registered as a built-in mesh.", primitiveID.c_str());
                }
                else
                {
                    ImGui::SetTooltip("Primitive '%s' is registered but not loaded.", primitiveID.c_str());
                }
            }
        }

        ImGui::EndPopup();
    }
}

namespace Quiet::Renderer::UI::Assets
{
    void DrawAssetBrowserToolbar(Editor::EditorState& editorState,
                                 const Project::ProjectContext& projectContext,
                                 const Vulkan::MeshLibrary& meshLibrary,
                                 const EditorFonts& fonts,
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

        const bool projectSourceActive = state.CurrentSource == AssetBrowserSource::Project;
        const std::size_t meshCount = CountAssetByType(registry, Project::AssetType::Mesh, state.CurrentSource);
        const std::size_t textureCount = CountAssetByType(registry, Project::AssetType::Texture, state.CurrentSource);
        const std::size_t audioCount = CountAssetByType(registry, Project::AssetType::Audio, state.CurrentSource);
        const std::size_t scriptCount = CountAssetByType(registry, Project::AssetType::Script, state.CurrentSource);

        const AssetDiagnosticsCounts diagnosticsCounts = CountAssetDiagnostics(projectContext, state.CurrentSource);
        const std::size_t issueCount = diagnosticsCounts.Issues;
        const std::size_t missingCount = diagnosticsCounts.Missing;
        const std::size_t externalCount = diagnosticsCounts.External;

        const float toolbarWidth = ImGui::GetContentRegionAvail().x;
        const bool useSecondaryRow = !fonts.HasIcons || toolbarWidth < SZ::Px(820.0f);
        const float compactButtonWidth = fonts.HasIcons ? ToolbarStyle::ButtonSize() : SZ::Px(92.0f);
        const ImVec2 compactButtonSize(compactButtonWidth, ToolbarStyle::ButtonHeight());

        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, SZ::Px(ImVec2(10.0f, 7.0f)));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, SZ::Px(ImVec2(8.0f, 8.0f)));

        if (projectSourceActive)
        {
            if (DrawToolbarButton(fonts, "AssetImport", Icons::FileImport, "Import", "Import Asset", false, true, compactButtonSize))
            {
                openImportPopup = true;
            }
        }
        else
        {
            const bool canCreatePrimitive = editorState.Scene.Mode == Editor::SceneMode::Edit;
            const char* primitiveTooltip = canCreatePrimitive
                ? "Create a built-in primitive actor at the world origin."
                : "Primitive actors can only be created in Edit mode.";

            if (DrawToolbarButton(
                fonts,
                "AssetCreatePrimitive",
                Icons::Local,
                "Create",
                primitiveTooltip,
                false,
                canCreatePrimitive,
                compactButtonSize))
            {
                ImGui::OpenPopup("##AssetBrowserCreatePrimitive");
            }

            DrawPrimitiveCreationPopup(editorState, projectContext, meshLibrary);
        }

        ImGui::SameLine();

        if (DrawToolbarButton(fonts, "AssetReload", Icons::Reload, "Reload", "Reload Assets", false, true, compactButtonSize))
        {
            editorState.QueueRequest(Editor::EditorRequestType::ReloadAssets);
        }

        const std::string actionsTooltip = selectedAssetCount > 0
            ? std::to_string(selectedAssetCount) + " selected - Asset Actions"
            : "Asset Actions";

        const bool clipboardCanPasteHere = projectSourceActive && clipboardAvailable;
        if (projectSourceActive && (selectedAssetCount > 0 || clipboardCanPasteHere))
        {
            ImGui::SameLine();

            if (DrawAssetToolbarDropdownButton(
                fonts,
                "AssetActions",
                Icons::More,
                "Actions",
                actionsTooltip.c_str(),
                "##AssetSelectionActions",
                selectedAssetCount > 0,
                compactButtonSize))
            {
                const bool editMode = editorState.Scene.Mode == Editor::SceneMode::Edit;
                const bool canEditProjectAssets = editMode && projectSourceActive;

                if (selectedAssetCount > 0)
                {
                    // COPY
                    const char* copyLabel = selectedAssetCount == 1 ? "Copy Asset" : "Copy Selection";
                    if (EditorMenuItem(copyLabel, "Ctrl+C", false, canEditProjectAssets))
                    {
                        operationRequest.Clear();
                        operationRequest.Action = AssetBrowserOperationAction::CopySelection;
                        operationRequest.AssetIDs = state.SelectedAssetIDs;
                    }

                    // CUT
                    const char* cutLabel = selectedAssetCount == 1 ? "Cut Asset" : "Cut Selection";
                    if (EditorMenuItem(cutLabel, "Ctrl+X", false, canEditProjectAssets))
                    {
                        operationRequest.Clear();
                        operationRequest.Action = AssetBrowserOperationAction::CutSelection;
                        operationRequest.AssetIDs = state.SelectedAssetIDs;
                    }

                    // RENAME
                    const Project::AssetRecord* renameRecord = selectedAssetCount == 1
                        ? Project::AssetRegistryIO::FindByID(projectContext.GetAssetRegistry(), state.SelectedAssetIDs.front())
                        : nullptr;

                    bool canRename = canEditProjectAssets && renameRecord && !renameRecord->BuiltIn && !renameRecord->Path.empty();
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
                    if (EditorMenuItem(duplicateLabel, "Ctrl+D", false, canEditProjectAssets))
                    {
                        operationRequest.Clear();
                        operationRequest.Action = AssetBrowserOperationAction::Duplicate;
                        operationRequest.AssetIDs = state.SelectedAssetIDs;
                        operationRequest.TargetFolder = state.CurrentAssetFolder;
                    }

                    ImGui::Separator();

                    // REMOVE
                    const char* removeLabel = selectedAssetCount == 1 ? "Remove from Registry..." : "Remove Selection from Registry...";
                    if (EditorMenuItem(removeLabel, nullptr, false, canEditProjectAssets))
                    {
                        RequestAssetRegistryRemoval(state, state.SelectedAssetIDs);
                    }

                    // DELETE
                    const char* deleteLabel = selectedAssetCount == 1 ? "Delete from Project..." : "Delete Selection from Project...";
                    if (EditorMenuItem(deleteLabel, "Delete", false, canEditProjectAssets))
                    {
                        RequestAssetProjectDeletion(state, state.SelectedAssetIDs);
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

                    if (EditorMenuItem(pasteLabel.c_str(), "Ctrl+V", false, canEditProjectAssets))
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

        const float secondaryControlsReserve = useSecondaryRow
            ? 0.0f
            : compactButtonWidth * 3.0f + ImGui::GetStyle().ItemSpacing.x * 4.0f + SZ::Px(8.0f);

        float searchAvailable = ImGui::GetContentRegionAvail().x - secondaryControlsReserve;
        const bool showClearButton = searchBuffer[0] != '\0' && searchAvailable >= SZ::Px(160.0f) + compactButtonWidth + ImGui::GetStyle().ItemSpacing.x;

        if (showClearButton)
        {
            searchAvailable -= compactButtonWidth + ImGui::GetStyle().ItemSpacing.x;
        }

        const float searchWidth = std::min(SZ::Px(320.0f), std::max(SZ::Px(100.0f), searchAvailable));
        ImGui::SetNextItemWidth(searchWidth);
        ImGui::InputTextWithHint("##AssetSearch", "Search assets...", searchBuffer.data(), searchBuffer.size());

        if (showClearButton)
        {
            ImGui::SameLine();

            if (DrawToolbarButton(fonts, "ClearAssetSearch", Icons::Close, "Clear", "Clear Search", false, true, compactButtonSize))
            {
                searchBuffer[0] = '\0';
            }
        }

        if (useSecondaryRow)
        {
            ImGui::Spacing();
        }
        else
        {
            ImGui::SameLine();
            ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
            ImGui::SameLine();
        }

        const std::size_t totalAssetCount = meshCount + textureCount + audioCount + scriptCount;
        const std::string filterTooltip = MakeAssetTypeFilterDropdownLabel(typeFilter, meshCount, textureCount, audioCount, scriptCount);

        if (DrawAssetToolbarDropdownButton(
            fonts,
            "AssetTypeFilter",
            Icons::Filter,
            "Filter",
            filterTooltip.c_str(),
            "##AssetTypeFilterDropdown",
            typeFilter != AssetBrowserTypeFilter::All,
            compactButtonSize))
        {
            const std::string allLabel = "All " + std::to_string(totalAssetCount);
            const std::string meshesLabel = "Meshes " + std::to_string(meshCount);
            const std::string texturesLabel = "Textures " + std::to_string(textureCount);
            const std::string audioLabel = "Audio " + std::to_string(audioCount);
            const std::string scriptsLabel = "Scripts " + std::to_string(scriptCount);

            // All
            if (EditorMenuItem(allLabel.c_str(), nullptr, typeFilter == AssetBrowserTypeFilter::All))
                typeFilter = AssetBrowserTypeFilter::All;

            // Meshes
            if (EditorMenuItem(meshesLabel.c_str(), nullptr, typeFilter == AssetBrowserTypeFilter::Meshes))
                typeFilter = AssetBrowserTypeFilter::Meshes;

            // Textures
            if (EditorMenuItem(texturesLabel.c_str(), nullptr, typeFilter == AssetBrowserTypeFilter::Textures))
                typeFilter = AssetBrowserTypeFilter::Textures;

            // Audio
            if (EditorMenuItem(audioLabel.c_str(), nullptr, typeFilter == AssetBrowserTypeFilter::Audio))
                typeFilter = AssetBrowserTypeFilter::Audio;

            // Scripts
            if (EditorMenuItem(scriptsLabel.c_str(), nullptr, typeFilter == AssetBrowserTypeFilter::Scripts))
                typeFilter = AssetBrowserTypeFilter::Scripts;

            ImGui::EndPopup();
        }

        ImGui::SameLine();

        const std::string diagnosticsTooltip = MakeDiagnosticsDropdownLabel(diagnosticsFilter, issueCount, missingCount, externalCount);
        if (DrawAssetToolbarDropdownButton(
            fonts,
            "AssetDiagnostics",
            Icons::Diagnostics,
            "Diag",
            diagnosticsTooltip.c_str(),
            "##AssetDiagnosticsDropdown",
            diagnosticsFilter != AssetBrowserDiagnosticsFilter::All,
            compactButtonSize))
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

        const char* viewIcon = viewMode == AssetBrowserViewMode::Browser ? Icons::Grid : Icons::List;
        std::string viewTooltip = std::string("View: ") + ToAssetBrowserViewModeLabel(viewMode);

        if (viewMode == AssetBrowserViewMode::Browser)
        {
            viewTooltip += " - ";
            viewTooltip += ToAssetBrowserThumbnailSizeLabel(thumbnailSize);
        }

        if (DrawAssetToolbarDropdownButton(
                fonts,
                "AssetView",
                viewIcon,
                "View",
                viewTooltip.c_str(),
                "##AssetViewModeDropdown",
                false,
                compactButtonSize))
        {
            if (EditorMenuItem("List", nullptr, viewMode == AssetBrowserViewMode::List))
            {
                viewMode = AssetBrowserViewMode::List;
            }

            if (EditorMenuItem("Browser", nullptr, viewMode == AssetBrowserViewMode::Browser))
            {
                viewMode = AssetBrowserViewMode::Browser;
            }

            if (viewMode == AssetBrowserViewMode::Browser)
            {
                ImGui::Separator();
                ImGui::TextDisabled("Thumbnail Size");

                if (EditorMenuItem("Tiny", nullptr, thumbnailSize == AssetBrowserThumbnailSize::Tiny))
                {
                    thumbnailSize = AssetBrowserThumbnailSize::Tiny;
                }

                if (EditorMenuItem("Small", nullptr, thumbnailSize == AssetBrowserThumbnailSize::Small))
                {
                    thumbnailSize =
                        AssetBrowserThumbnailSize::Small;
                }

                if (EditorMenuItem("Medium", nullptr, thumbnailSize == AssetBrowserThumbnailSize::Medium))
                {
                    thumbnailSize = AssetBrowserThumbnailSize::Medium;
                }

                if (EditorMenuItem("Large", nullptr, thumbnailSize == AssetBrowserThumbnailSize::Large))
                {
                    thumbnailSize = AssetBrowserThumbnailSize::Large;
                }

                if (EditorMenuItem("Extra Large", nullptr, thumbnailSize == AssetBrowserThumbnailSize::ExtraLarge))
                {
                    thumbnailSize = AssetBrowserThumbnailSize::ExtraLarge;
                }
            }

            ImGui::EndPopup();
        }

        ImGui::PopStyleVar(2);
    }
}

#include "Renderer/UI/Assets/ImGuiAssetBrowserToolbar.h"

#include <cstddef>
#include <string>

#include <imgui.h>
#include <imgui_internal.h>

#include "Renderer/UI/Assets/ImGuiAssetBrowserUtils.h"
#include "Renderer/UI/ImGuiLayer.h"
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

}

namespace Quiet::Renderer::UI::Assets
{
    void DrawAssetBrowserToolbar(
        Editor::EditorState &editorState,
        const Project::AssetRegistry &registry,
        std::array<char, 128> &searchBuffer,
        AssetBrowserTypeFilter &typeFilter,
        AssetBrowserViewMode &viewMode,
        AssetBrowserThumbnailSize &thumbnailSize,
        bool &openImportPopup)
    {
        const std::size_t meshCount = CountAssetByType(registry, Project::AssetType::Mesh);
        const std::size_t textureCount = CountAssetByType(registry, Project::AssetType::Texture);

        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, SZ::Px(ImVec2(10.0f, 7.0f)));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, SZ::Px(ImVec2(8.0f, 8.0f)));

        if (EditorButton("Import", SZ::Px(ImVec2(86.0f, 0.0f))))
            openImportPopup = true;

        ImGui::SameLine();

        if (EditorButton("Reload", SZ::Px(ImVec2(86.0f, 0.0f))))
            editorState.QueueRequest(Editor::EditorRequestType::ReloadAssets);

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

            if (EditorMenuItem(allLabel.c_str(), nullptr, typeFilter == AssetBrowserTypeFilter::All))
                typeFilter = AssetBrowserTypeFilter::All;

            if (EditorMenuItem(meshesLabel.c_str(), nullptr, typeFilter == AssetBrowserTypeFilter::Meshes))
                typeFilter = AssetBrowserTypeFilter::Meshes;

            if (EditorMenuItem(texturesLabel.c_str(), nullptr, typeFilter == AssetBrowserTypeFilter::Textures))
                typeFilter = AssetBrowserTypeFilter::Textures;

            ImGui::EndPopup();
        }

        ImGui::SameLine();

        const std::string viewDropdownLabel = std::string("View: ") + ToAssetBrowserViewModeLabel(viewMode);

        if (DrawAssetToolbarDropdownButton(viewDropdownLabel, "##AssetViewModeDropdown", SZ::Px(ImVec2(142.0f, 0.0f))))
        {
            if (EditorMenuItem("List", nullptr, viewMode == AssetBrowserViewMode::List))
                viewMode = AssetBrowserViewMode::List;

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
                if (EditorMenuItem("Small", nullptr, thumbnailSize == AssetBrowserThumbnailSize::Small))
                    thumbnailSize = AssetBrowserThumbnailSize::Small;

                if (EditorMenuItem("Medium", nullptr, thumbnailSize == AssetBrowserThumbnailSize::Medium))
                    thumbnailSize = AssetBrowserThumbnailSize::Medium;

                if (EditorMenuItem("Large", nullptr, thumbnailSize == AssetBrowserThumbnailSize::Large))
                    thumbnailSize = AssetBrowserThumbnailSize::Large;

                ImGui::EndPopup();
            }
        }

        ImGui::PopStyleVar(2);
    }
}

#pragma once

#include <string>
#include <vector>
#include <imgui.h>

#include "Quiet/Project/AssetRegistry.h"
#include "Renderer/UI/Assets/ImGuiAssetBrowserTypes.h"

namespace Quiet::Renderer::UI::Assets
{
    ImVec4 MakeStatusColor(const std::string& status);

    void DrawPillLabel(const char* label, const ImVec4& textColor, const ImVec4& backgroundColor);

    void DrawAssetTypePill(Project::AssetType type);
    void DrawAssetStatusPill(const std::string& status);
    void DrawAssetStatePill(const std::string& state);
    void DrawAssetDiagnosticPill(const std::string& label, bool missing);

    bool DrawAssetTableCellHitTarget(const char* id, float rowHeight);

    struct AssetBrowserDragPayload
    {
        // The card that initiated the drag. Inspector and Viewport
        // assignment continue to use this primary asset.
        std::string AssetID{};

        // Complete selection being dragged. Folder move operations use
        // every asset in this collection.
        std::vector<std::string> AssetIDs{};
        bool Delivered = false;

        [[nodiscard]] bool IsValid() const
        {
            return !AssetID.empty() && !AssetIDs.empty();
        }
    };

    void DrawAssetBrowserDragSource(const Project::AssetRecord& record, const AssetBrowserState& state, bool enabled);

    [[nodiscard]] bool ReadAssetBrowserDrop(AssetBrowserDragPayload& outPayload, ImGuiDragDropFlags flags = ImGuiDragDropFlags_None);
    [[nodiscard]] bool AcceptAssetBrowserDrop(std::vector<std::string>& outAssetIDs);

    struct AssetBrowserCardMetrics
    {
        float TargetCardWidth = 184.0f;
        float CardHeight = 178.0f;

        float Padding = 10.0f;
        float PreviewHeight = 96.0f;
        float PreviewInnerPadding = 8.0f;

        float GridSpacing = 10.0f;
        float CardRounding = 10.0f;
        float PreviewRounding = 8.0f;

        float ContentGap = 8.0f;
        float SecondaryGap = 3.0f;
        float MetadataGap = 7.0f;

        bool ShowMetadataPill = true;
        bool ShowSecondaryLine = false;
        bool ShowDiagnosticPill = true;
        bool CompactPreview = false;
    };

    AssetBrowserCardMetrics MakeAssetBrowserCardMetrics(AssetBrowserThumbnailSize thumbnailSize);

    void DrawAssetPaddedTooltip(const std::string& text);
    std::string MakeEllipsizedText(const std::string& text, float maxWidth);
    void DrawAssetCardTextLine(const std::string& text, const ImVec2& min, const ImVec2& max, const ImVec4& color);
}
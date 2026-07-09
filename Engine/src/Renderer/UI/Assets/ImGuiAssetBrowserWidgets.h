#pragma once

#include <string>
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

    bool DrawAssetTableCellHitTarget(const char* id, float rowHeight);

    struct AssetBrowserCardMetrics
    {
        float DesiredColumnWidth = 178.0f;
        float MinimumCardWidth = 150.0f;
        float CardHeight = 166.0f;
        float Padding = 10.0f;
        float PreviewHeight = 86.0f;
    };

    AssetBrowserCardMetrics MakeAssetBrowserCardMetrics(AssetBrowserThumbnailSize thumbnailSize);

    void DrawAssetPaddedTooltip(const std::string& text);
    std::string MakeEllipsizedText(const std::string& text, float maxWidth);
    void DrawAssetCardTextLine(const std::string& text, const ImVec2& min, const ImVec2& max, const ImVec4& color);
}
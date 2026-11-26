#include "Renderer/UI/Assets/ImGuiAssetBrowserWidgets.h"

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <type_traits>
#include <string>
#include <utility>
#include <vector>

#include "Renderer/UI/Assets/ImGuiAssetBrowserUtils.h"
#include "Renderer/UI/ImGuiTheme.h"

namespace
{
    namespace SZ = Quiet::Renderer::UI::EditorSizing;
    namespace Assets = Quiet::Renderer::UI::Assets;

    constexpr const char* kAssetBrowserPayloadType = "QUIET_ASSET_BROWSER_UUIDS";

    std::vector<Quiet::Core::UUID> BuildDragAssetIDs(const Quiet::Project::AssetRecord& record, const Assets::AssetBrowserState& state)
    {
        std::vector<Quiet::Core::UUID> assetIDs;
        assetIDs.push_back(record.ID);

        if (!Assets::IsAssetSelected(state, record.ID))
        {
            return assetIDs;
        }

        assetIDs.reserve(state.SelectedAssetIDs.size() );
        for (Quiet::Core::UUID selectedAssetID : state.SelectedAssetIDs)
        {
            if (!selectedAssetID.IsValid() || selectedAssetID == record.ID) continue;
            assetIDs.push_back(selectedAssetID);
        }

        return assetIDs;
    }

    bool TryReadAssetBrowserPayload(const ImGuiPayload* payload, std::vector<Quiet::Core::UUID>& outAssetIDs)
    {
        static_assert(std::is_trivially_copyable_v<Quiet::Core::UUID>);
        outAssetIDs.clear();
        if (!payload || !payload->Data || payload->DataSize <= 0) return false;

        const auto byteCount = static_cast<std::size_t>(payload->DataSize);
        if (byteCount % sizeof(Quiet::Core::UUID) != 0) return false;

        std::vector<Quiet::Core::UUID> assetIDs(byteCount / sizeof(Quiet::Core::UUID));
        std::memcpy(assetIDs.data(), payload->Data, byteCount);
        if (std::any_of(assetIDs.begin(), assetIDs.end(),
                [](Quiet::Core::UUID id) { return !id.IsValid(); }))
        {
            return false;
        }

        outAssetIDs = std::move(assetIDs);
        return true;
    }

}

namespace Quiet::Renderer::UI::Assets
{
    ImVec4 MakeStatusColor(const std::string& status)
    {
        if (status == "Loaded" ||
            status == "GPU Ready" ||
            status == "Loaded Metadata" ||
            status == "Valid")
            return EditorColor::Success;

        if (status == "Error")
            return EditorColor::Error;

        if (status == "Registered")
            return EditorColor::Warning;

        if (status == "Missing Built-In" ||
            status == "Missing Path" ||
            status == "Missing File" ||
            status == "Invalid Source" ||
            status == "Unknown")
            return EditorColor::Error;

        return EditorColor::TextMuted;
    }

    void DrawPillLabel(const char* label, const ImVec4& textColor, const ImVec4& backgroundColor)
    {
        const ImVec2 textSize = ImGui::CalcTextSize(label);
        const ImVec2 padding = SZ::Px(ImVec2(8.0f, 3.0f));

        const ImVec2 min = ImGui::GetCursorScreenPos();
        const ImVec2 max(
            min.x + textSize.x + padding.x * 2.0f,
            min.y + textSize.y + padding.y * 2.0f
        );

        ImDrawList* drawList = ImGui::GetWindowDrawList();

        drawList->AddRectFilled(
            min,
            max,
            ImGui::ColorConvertFloat4ToU32(backgroundColor),
            SZ::Px(999.0f)
        );

        drawList->AddText(
            ImVec2(min.x + padding.x, min.y + padding.y),
            ImGui::ColorConvertFloat4ToU32(textColor),
            label
        );

        ImGui::Dummy(ImVec2(max.x - min.x, max.y - min.y));
    }

    void DrawAssetTypePill(const Project::AssetType type)
    {
        switch (type)
        {
            case Project::AssetType::Mesh:
                DrawPillLabel("Mesh", EditorColor::Accent, ImVec4(0.040f, 0.075f, 0.135f, 1.0f));
                break;

            case Project::AssetType::Texture:
                DrawPillLabel("Texture", EditorColor::Success, ImVec4(0.035f, 0.105f, 0.065f, 1.0f));
                break;

            case Project::AssetType::Audio:
                DrawPillLabel("Audio", EditorColor::Warning, ImVec4(0.130f, 0.085f, 0.025f, 1.0f));
                break;

            case Project::AssetType::Script:
                DrawPillLabel("Script", EditorColor::Accent, EditorColor::Background3);
                break;

            case Project::AssetType::Unknown:
            default:
                DrawPillLabel("Unknown", EditorColor::TextMuted, EditorColor::Background3);
                break;
        }
    }

    void DrawAssetStatusPill(const std::string& status)
    {
        const ImVec4 statusColor = MakeStatusColor(status);
        DrawPillLabel(status.c_str(), statusColor, ImVec4(0.055f, 0.065f, 0.085f, 1.0f));
    }

    void DrawAssetStatePill(const std::string& state)
    {
        ImVec4 text = EditorColor::TextMuted;
        ImVec4 background = EditorColor::Background3;

        if (state.find("Error") != std::string::npos)
        {
            text = EditorColor::Error;
            background = ImVec4(0.140f, 0.045f, 0.050f, 1.0f);
        }
        else if (state.find("Missing") != std::string::npos)
        {
            text = EditorColor::Error;
            background = ImVec4(0.140f, 0.045f, 0.050f, 1.0f);
        }
        else if (state.find("External") != std::string::npos)
        {
            text = EditorColor::Warning;
            background = ImVec4(0.130f, 0.095f, 0.030f, 1.0f);
        }
        else if (state.find("Built-in") != std::string::npos)
        {
            text = EditorColor::Accent;
            background = ImVec4(0.040f, 0.075f, 0.135f, 1.0f);
        }
        else if (state.find("In Use") != std::string::npos)
        {
            text = EditorColor::Warning;
            background = ImVec4(0.130f, 0.095f, 0.030f, 1.0f);
        }
        else if (state.find("Removable") != std::string::npos ||
                 state.find("Ready") != std::string::npos)
        {
            text = EditorColor::Success;
            background = ImVec4(0.035f, 0.105f, 0.065f, 1.0f);
        }

        DrawPillLabel(state.c_str(), text, background);
    }

    void DrawAssetDiagnosticPill(const std::string &label, bool missing)
    {
        if (label.empty()) return;
        const ImVec4 textColor = missing ? EditorColor::Error : EditorColor::Warning;
        const ImVec4 backgroundColor = missing ? ImVec4(0.140f, 0.045f, 0.050f, 0.96f) : ImVec4(0.130f, 0.095f, 0.030f, 0.96f);

        DrawPillLabel(label.c_str(), textColor, backgroundColor);
    }

    bool DrawAssetTableCellHitTarget(const char* id, const float rowHeight)
    {
        const float cellWidth = ImGui::GetContentRegionAvail().x;
        if (cellWidth <= SZ::Px(4.0f))
            return false;

        ImGui::InvisibleButton(id, ImVec2(cellWidth, rowHeight));
        if (ImGui::IsItemHovered())
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

        return ImGui::IsItemClicked(ImGuiMouseButton_Left);
    }

    void DrawAssetBrowserDragSource(const Project::AssetRecord& record, const AssetBrowserState& state, const bool enabled)
    {
        if (!enabled || (!record.BuiltIn && record.Path.empty())) return;

        const ImVec2 previewPadding = SZ::Px(ImVec2(14.0f, 11.0f));

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, previewPadding);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, SZ::Px(8.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, SZ::Px(1.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, SZ::Px(ImVec2(5.0f, 5.0f)));

        ImGui::PushStyleColor(ImGuiCol_PopupBg, EditorColor::Background2);
        ImGui::PushStyleColor(ImGuiCol_Border, EditorColor::BorderSoft);

        if (!ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceNoDisableHover))
        {
            ImGui::PopStyleColor(2);
            ImGui::PopStyleVar(4);
            return;
        }

        const std::vector<Quiet::Core::UUID> dragAssetIDs = BuildDragAssetIDs(record, state);
        ImGui::SetDragDropPayload(kAssetBrowserPayloadType, dragAssetIDs.data(),
                                  dragAssetIDs.size() * sizeof(Quiet::Core::UUID));

        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + SZ::Px(280.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, EditorColor::Text);

        if (dragAssetIDs.size() == 1)
        {
            ImGui::TextUnformatted("Asset");
        }
        else
        {
            ImGui::Text("%zu Assets", dragAssetIDs.size());
        }

        ImGui::PopStyleColor();

        ImGui::Spacing();

        ImGui::PushStyleColor(ImGuiCol_Text, EditorColor::Accent);
        ImGui::TextWrapped("%s", record.Name.c_str());
        ImGui::PopStyleColor();

        ImGui::PushStyleColor(ImGuiCol_Text, EditorColor::TextMuted);
        if (dragAssetIDs.size() > 1)
        {
            ImGui::Text("+ %zu more selected", dragAssetIDs.size() - 1);
        }
        else
        {
            ImGui::TextWrapped("%s", record.BuiltIn ? "<built-in>" : record.Path.c_str());
        }
        ImGui::PopStyleColor();

        ImGui::PopTextWrapPos();
        ImGui::EndDragDropSource();

        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(4);
    }

    bool ReadAssetBrowserDrop(AssetBrowserDragPayload& outPayload, const ImGuiDragDropFlags flags)
    {
        outPayload = {};

        if (!ImGui::BeginDragDropTarget()) return false;

        const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(kAssetBrowserPayloadType, flags);
        if (payload)
        {
            outPayload.Delivered = payload->IsDelivery();
            TryReadAssetBrowserPayload(payload, outPayload.AssetIDs);
            if (!outPayload.AssetIDs.empty())
            {
                outPayload.AssetID = outPayload.AssetIDs.front();
            }
        }

        ImGui::EndDragDropTarget();
        return outPayload.IsValid();
    }

    bool AcceptAssetBrowserDrop(std::vector<Quiet::Core::UUID>& outAssetIDs)
    {
        outAssetIDs.clear();

        AssetBrowserDragPayload payload{};
        if (!ReadAssetBrowserDrop(payload) || !payload.Delivered) return false;

        outAssetIDs = std::move(payload.AssetIDs);
        return !outAssetIDs.empty();
    }

    AssetBrowserCardMetrics MakeAssetBrowserCardMetrics(const AssetBrowserThumbnailSize thumbnailSize)
    {
        switch (thumbnailSize)
        {
            case AssetBrowserThumbnailSize::Tiny:
                return {
                    SZ::Px(108.0f),     // TargetCardWidth
                    SZ::Px(98.0f),      // CardHeight
                    SZ::Px(6.0f),       // Padding
                    SZ::Px(48.0f),      // PreviewHeight
                    SZ::Px(4.0f),       // PreviewInnerPadding
                    SZ::Px(6.0f),       // GridSpacing
                    SZ::Px(8.0f),       // CardRounding
                    SZ::Px(6.0f),       // PreviewRounding
                    SZ::Px(5.0f),       // ContentGap
                    SZ::Px(2.0f),       // SecondaryGap
                    SZ::Px(5.0f),       // MetadataGap
                    false,                   // ShowMetadataPill
                    false,                   // ShowSecondaryLine
                    false,                   // ShowDiagnosticPill
                    true                     // CompactPreview
                };

            case AssetBrowserThumbnailSize::Small:
                return {
                    SZ::Px(142.0f),
                    SZ::Px(136.0f),
                    SZ::Px(8.0f),
                    SZ::Px(68.0f),
                    SZ::Px(6.0f),
                    SZ::Px(8.0f),
                    SZ::Px(9.0f),
                    SZ::Px(7.0f),
                    SZ::Px(6.0f),
                    SZ::Px(2.0f),
                    SZ::Px(6.0f),
                    true,
                    false,
                    false,
                    true
                };

            case AssetBrowserThumbnailSize::Large:
                return {
                    SZ::Px(246.0f),
                    SZ::Px(250.0f),
                    SZ::Px(12.0f),
                    SZ::Px(148.0f),
                    SZ::Px(10.0f),
                    SZ::Px(12.0f),
                    SZ::Px(12.0f),
                    SZ::Px(9.0f),
                    SZ::Px(8.0f),
                    SZ::Px(3.0f),
                    SZ::Px(8.0f),
                    true,
                    true,
                    true,
                    false
                };

            case AssetBrowserThumbnailSize::ExtraLarge:
                return {
                    SZ::Px(322.0f),
                    SZ::Px(322.0f),
                    SZ::Px(14.0f),
                    SZ::Px(210.0f),
                    SZ::Px(12.0f),
                    SZ::Px(14.0f),
                    SZ::Px(14.0f),
                    SZ::Px(10.0f),
                    SZ::Px(10.0f),
                    SZ::Px(4.0f),
                    SZ::Px(8.0f),
                    true,
                    true,
                    true,
                    false
                };

            case AssetBrowserThumbnailSize::Medium:
            default:
                return {
                    SZ::Px(184.0f),
                    SZ::Px(178.0f),
                    SZ::Px(10.0f),
                    SZ::Px(96.0f),
                    SZ::Px(8.0f),
                    SZ::Px(10.0f),
                    SZ::Px(10.0f),
                    SZ::Px(8.0f),
                    SZ::Px(8.0f),
                    SZ::Px(3.0f),
                    SZ::Px(7.0f),
                    true,
                    false,
                    true,
                    false
                };
        }
    }

    void DrawAssetPaddedTooltip(const std::string& text)
    {
        if (text.empty()) return;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, SZ::Px(ImVec2(12.0f, 9.0f)));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, SZ::Px(8.0f));

        ImGui::PushStyleColor(ImGuiCol_PopupBg, EditorColor::Background2);
        ImGui::PushStyleColor(ImGuiCol_Border, EditorColor::BorderSoft);

        ImGui::BeginTooltip();
        {
            ImGui::PushTextWrapPos(SZ::Px(520.0f));

            ImGui::PushStyleColor(ImGuiCol_Text, EditorColor::Text);
            ImGui::TextWrapped("%s", text.c_str());
            ImGui::PopStyleColor();

            ImGui::PopTextWrapPos();
        }
        ImGui::EndTooltip();

        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(2);
    }

    std::string MakeEllipsizedText(const std::string& text, const float maxWidth)
    {
        if (text.empty()) return {};
        if (ImGui::CalcTextSize(text.c_str()).x <= maxWidth) return text;

        constexpr const char* ellipsis = "...";
        const float ellipsisWidth = ImGui::CalcTextSize(ellipsis).x;

        if (ellipsisWidth >= maxWidth) return ellipsis;

        std::size_t low = 0;
        std::size_t high = text.size();

        while (low < high)
        {
            const std::size_t mid = (low + high + 1) / 2;
            const std::string candidate = text.substr(0, mid) + ellipsis;

            if (ImGui::CalcTextSize(candidate.c_str()).x <= maxWidth) low = mid;
            else high = mid - 1;
        }

        return text.substr(0, low) + ellipsis;
    }

    void DrawAssetCardTextLine(const std::string& text, const ImVec2& min, const ImVec2& max, const ImVec4& color)
    {
        const float maxWidth = std::max(0.0f, max.x - min.x);
        const std::string visibleText = MakeEllipsizedText(text, maxWidth);

        ImDrawList* drawList = ImGui::GetWindowDrawList();

        drawList->PushClipRect(min, max, true);
        drawList->AddText(min, ImGui::ColorConvertFloat4ToU32(color), visibleText.c_str());
        drawList->PopClipRect();
    }
}
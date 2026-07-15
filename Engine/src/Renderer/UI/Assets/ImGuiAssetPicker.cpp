#include "Renderer/UI/Assets/ImGuiAssetPicker.h"

#include <algorithm>
#include <array>
#include <string>
#include <vector>
#include <imgui.h>

#include "ImGuiAssetBrowserUtils.h"
#include "ImGuiAssetBrowserWidgets.h"
#include "Quiet/Project/AssetRecord.h"
#include "Renderer/UI/ImGuiTexturePreview.h"
#include "Renderer/UI/ImGuiTheme.h"

namespace
{
    namespace QUI = Quiet::Renderer::UI;
    namespace SZ = Quiet::Renderer::UI::EditorSizing;
    namespace Assets = Quiet::Renderer::UI::Assets;

    std::array<char, 128> g_AssetPickerSearchBuffer;
    ImGuiID g_ActiveAssetPickerID = 0;

    bool MatchesAssetPickerSearch(const Quiet::Project::AssetRecord& record, const std::string& searchQuery)
    {
        if (searchQuery.empty()) return true;
        return Assets::ContainsCaseInsensitive(record.ID, searchQuery) || Assets::ContainsCaseInsensitive(record.Path, searchQuery);
    }

    void DrawMeshPickerGlyph(ImDrawList* drawList, const ImVec2& min, const ImVec2& max, const ImVec4& color)
    {
        const ImVec2 center((min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f);

        const float halfWidth = (max.x - min.x) * 0.28f;
        const float halfHeight = (max.y - min.y) * 0.30f;

        const ImVec2 top(center.x, center.y - halfHeight);
        const ImVec2 left(center.x - halfWidth, center.y + halfHeight);
        const ImVec2 right(center.x + halfWidth, center.y + halfHeight);

        const ImU32 packedColor = ImGui::ColorConvertFloat4ToU32(color);

        drawList->AddTriangle(
            top,
            right,
            left,
            packedColor,
            SZ::Px(2.0f)
        );

        drawList->AddLine(
            top,
            center,
            packedColor,
            SZ::Px(2.0f)
        );

        drawList->AddLine(
            left,
            center,
            packedColor,
            SZ::Px(2.0f)
        );

        drawList->AddLine(
            right,
            center,
            packedColor,
            SZ::Px(2.0f)
        );
    }

    void DrawTexturePickerFallback(ImDrawList* drawList, const ImVec2& min, const ImVec2& max, const ImVec4& color)
    {
        const char* label = "TEX";

        const ImVec2 textSize = ImGui::CalcTextSize(label);
        const ImVec2 textPosition(min.x + ((max.x - min.x) - textSize.x) * 0.5f, min.y + ((max.y - min.y) - textSize.y) * 0.5f);

        drawList->AddText(
            textPosition,
            ImGui::ColorConvertFloat4ToU32(color),
            label
        );
    }

    std::string MakePickerStatusLabel(
        const Quiet::Project::AssetRecord& record,
        const Assets::AssetPathDiagnostics& diagnostics,
        const Assets::AssetPickerOptions& options)
    {
        if (diagnostics.IsMissing())
            return Assets::MakeAssetPathDiagnosticLabel(diagnostics);

        if (record.Type == Quiet::Project::AssetType::Mesh && options.MeshLibrary && !options.MeshLibrary->TryGet(record.ID))
            return "Not Loaded";

        if (record.Type == Quiet::Project::AssetType::Texture && options.TextureLibrary)
        {
            const auto textureStats = options.TextureLibrary->GetStats(record.ID);
            if (!textureStats.has_value())  return "Not Loaded";
            if (!textureStats->GpuReady)    return "Metadata";
            return "GPU Ready";
        }

        if (diagnostics.External)   return "External";
        if (record.BuiltIn)         return "Built-in";
        return "Available";
    }

    ImVec4 MakePickerStatusColor(const std::string& status)
    {
        if (status == "Missing Path" ||
            status == "Missing File" ||
            status == "Invalid Source")
            return QUI::EditorColor::Error;

        if (status == "External" ||
            status == "Not Loaded" ||
            status == "Metadata")
            return QUI::EditorColor::Warning;

        if (status == "GPU Ready" ||
            status == "Available")
            return QUI::EditorColor::Success;

        return QUI::EditorColor::TextMuted;
    }

    bool IsAssetPickerRecordAssignable(
        const Quiet::Project::AssetRecord& record,
        const Assets::AssetPathDiagnostics& diagnostics,
        const Assets::AssetPickerOptions& options)
    {
        if (diagnostics.IsMissing()) return false;
        if (record.Type == Quiet::Project::AssetType::Mesh && options.MeshLibrary && !options.MeshLibrary->TryGet(record.ID)) return false;
        return true;
    }

    std::string MakeAssetPickerDisabledReason(
        const Quiet::Project::AssetRecord& record,
        const Assets::AssetPathDiagnostics& diagnostics,
        const Assets::AssetPickerOptions& options)
    {
        if (diagnostics.IsMissing())
            return Assets::MakeAssetPathDiagnosticDescription(diagnostics);

        if (record.Type == Quiet::Project::AssetType::Mesh && options.MeshLibrary && !options.MeshLibrary->TryGet(record.ID))
            return "This mesh is registered but not loaded. Reload project assets before assigning it.";

        return {};
    }

    bool DrawAssetPickerRow(
        const Quiet::Project::AssetRecord& record,
        const Quiet::Project::ProjectContext& projectContext,
        const Assets::AssetPickerOptions& options,
        const bool selected)
    {
        const Assets::AssetPathDiagnostics diagnostics = Assets::InspectAssetPath(projectContext, record);

        const bool assignable = IsAssetPickerRecordAssignable(record, diagnostics, options);

        const std::string status = MakePickerStatusLabel(record, diagnostics, options);
        const ImVec4 statusColor = MakePickerStatusColor(status);

        const float rowHeight = SZ::Px(58.0f);
        const ImVec2 rowMin = ImGui::GetCursorScreenPos();
        const std::string rowID = "##AssetPickerRow_" + record.ID;
        const bool pressed = QUI::EditorSelectable(rowID.c_str(), selected, ImGuiSelectableFlags_None, ImVec2(0.0f, rowHeight), assignable);
        const ImVec2 rowMax = ImGui::GetItemRectMax();

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const float padding = SZ::Px(7.0f);

        const float iconSize = rowHeight - padding * 2.0f;
        const ImVec2 iconMin(rowMin.x + padding, rowMin.y + padding);
        const ImVec2 iconMax(iconMin.x + iconSize, iconMin.y + iconSize);

        drawList->AddRectFilled(
            iconMin,
            iconMax,
            ImGui::ColorConvertFloat4ToU32(
                QUI::EditorColor::Background3
            ),
            SZ::Px(6.0f)
        );

        drawList->AddRect(
            iconMin,
            iconMax,
            ImGui::ColorConvertFloat4ToU32(
                diagnostics.IsMissing()
                    ? QUI::EditorColor::Error
                    : diagnostics.External
                        ? QUI::EditorColor::Warning
                        : QUI::EditorColor::BorderSoft
            ),
            SZ::Px(6.0f)
        );

        bool previewDrawn = false;

        if (record.Type == Quiet::Project::AssetType::Texture && options.TextureLibrary)
        {
            const auto textureStats = options.TextureLibrary->GetStats(record.ID);
            if (textureStats.has_value() && textureStats->GpuReady)
            {
                const ImTextureID previewID = QUI::GetOrCreateTexturePreviewID(record.ID, *options.TextureLibrary);
                if (previewID)
                {
                    drawList->AddImage(previewID, iconMin, iconMax);
                    previewDrawn = true;
                }
            }
        }

        if (!previewDrawn)
        {
            if (record.Type == Quiet::Project::AssetType::Mesh)
            {
                DrawMeshPickerGlyph(drawList, iconMin, iconMax, assignable ? QUI::EditorColor::AccentHover : QUI::EditorColor::TextDim);
            }
            else
            {
                DrawTexturePickerFallback(drawList, iconMin, iconMax, assignable ? QUI::EditorColor::AccentHover : QUI::EditorColor::TextDim);
            }
        }

        const float textX = iconMax.x + SZ::Px(10.0f);
        const float statusWidth = ImGui::CalcTextSize(status.c_str()).x;
        const float textRight = rowMax.x - padding - statusWidth - SZ::Px(12.0f);

        const float availableTextWidth = std::max(SZ::Px(40.0f), textRight - textX);

        const std::string displayID = Assets::MakeEllipsizedText(record.ID, availableTextWidth);
        const std::string displayPath = Assets::MakeEllipsizedText(record.Path.empty() ? std::string("<built-in>"): record.Path, availableTextWidth);

        drawList->AddText(
            ImVec2(
                textX,
                rowMin.y + SZ::Px(8.0f)
            ),
            ImGui::ColorConvertFloat4ToU32(
                assignable
                    ? QUI::EditorColor::Text
                    : QUI::EditorColor::TextDim
            ),
            displayID.c_str()
        );

        drawList->AddText(
            ImVec2(
                textX,
                rowMin.y + SZ::Px(31.0f)
            ),
            ImGui::ColorConvertFloat4ToU32(
                assignable
                    ? QUI::EditorColor::TextMuted
                    : QUI::EditorColor::TextDim
            ),
            displayPath.c_str()
        );

        drawList->AddText(
            ImVec2(
                rowMax.x -
                    padding -
                    statusWidth,
                rowMin.y + SZ::Px(19.0f)
            ),
            ImGui::ColorConvertFloat4ToU32(
                statusColor
            ),
            status.c_str()
        );

        if (selected) ImGui::SetItemDefaultFocus();
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
        {
            if (!assignable)
            {
                Assets::DrawAssetPaddedTooltip(MakeAssetPickerDisabledReason(record, diagnostics, options));
            }
            else if (diagnostics.HasIssue())
            {
                Assets::DrawAssetPaddedTooltip(Assets::MakeAssetPathDiagnosticDescription(diagnostics));
            }
            else
            {
                Assets::DrawAssetPaddedTooltip(record.Path.empty() ? record.ID : record.Path);
            }
        }

        return pressed;
    }
}

namespace Quiet::Renderer::UI::Assets
{
    bool DrawAssetPicker(
        const char *id,
        const Project::ProjectContext &projectContext,
        const AssetPickerOptions &options,
        const std::string &currentAssetID,
        std::string &outSelectedAssetID)
    {
        const Project::AssetRecord* currentRecord = currentAssetID.empty()
            ? nullptr
            : Project::AssetRegistryIO::FindByID(projectContext.GetAssetRegistry(), currentAssetID);

        const bool currentSelectionInvalid = !currentAssetID.empty() && (!currentRecord || currentRecord->Type != options.Type);
        const std::string previewLabel =
            currentAssetID.empty()
                ? "None"
                : currentSelectionInvalid
                    ? "Missing: " + currentAssetID
                    : currentAssetID;

        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
        ImGui::SetNextWindowSizeConstraints(SZ::Px(ImVec2(380.0f, 220.0f)), SZ::Px(ImVec2(560.0f, 460.0f)));

        if (currentSelectionInvalid)
        {
            ImGui::PushStyleColor(ImGuiCol_Text, EditorColor::Error);
        }

        const bool pickerOpen = EditorBeginCombo(id,previewLabel.c_str(), ImGuiComboFlags_HeightLargest);
        if (currentSelectionInvalid) ImGui::PopStyleColor();
        if (!pickerOpen) return false;
        const ImGuiID pickerID = ImGui::GetID(id);

        if (ImGui::IsWindowAppearing() || g_ActiveAssetPickerID != pickerID)
        {
            g_ActiveAssetPickerID = pickerID;
            g_AssetPickerSearchBuffer.fill('\0');
            ImGui::SetKeyboardFocusHere();
        }

        ImGui::SetNextItemWidth(-1.0f);
        ImGui::InputTextWithHint("##AssetPickerSearch", "Search by asset ID or path...", g_AssetPickerSearchBuffer.data(), g_AssetPickerSearchBuffer.size());

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (currentSelectionInvalid)
        {
            ImGui::PushStyleColor(
                ImGuiCol_Text,
                EditorColor::Error
            );

            ImGui::TextWrapped(
                "The currently assigned asset is no longer "
                "present in the registry."
            );

            ImGui::PopStyleColor();

            ImGui::Spacing();
        }

        std::vector<const Project::AssetRecord*> records =
            Project::AssetRegistryIO::GetAssetsByType(
                projectContext.GetAssetRegistry(),
                options.Type
            );

        std::sort(
            records.begin(),
            records.end(),
            [](const Project::AssetRecord* left,
               const Project::AssetRecord* right)
            {
                if (!left)
                    return false;

                if (!right)
                    return true;

                return ToLowerCopy(left->ID) <
                       ToLowerCopy(right->ID);
            }
        );

        const std::string searchQuery = g_AssetPickerSearchBuffer.data();

        bool changed = false;
        std::size_t visibleRecordCount = 0;

        ImGui::BeginChild("##AssetPickerResults", ImVec2(0.0f, SZ::Px(300.0f)), ImGuiChildFlags_Borders);
        {
            if (options.AllowNone && (searchQuery.empty() || ContainsCaseInsensitive("None", searchQuery)))
            {
                if (EditorSelectable("None", currentAssetID.empty(), ImGuiSelectableFlags_None, ImVec2(0.0f, SZ::Px(36.0f))))
                {
                    outSelectedAssetID.clear();
                    changed = true;
                    ImGui::CloseCurrentPopup();
                }

                if (currentAssetID.empty()) ImGui::SetItemDefaultFocus();
                ++visibleRecordCount;
            }

            for (const Project::AssetRecord* record : records)
            {
                if (!record || !record->IsValid() || !MatchesAssetPickerSearch(*record, searchQuery)) continue;
                if (visibleRecordCount > 0) ImGui::Separator();

                const bool selected = record->ID == currentAssetID;
                if (DrawAssetPickerRow(*record, projectContext, options, selected))
                {
                    outSelectedAssetID = record->ID;
                    changed = true;
                    ImGui::CloseCurrentPopup();
                }

                ++visibleRecordCount;
            }

            if (visibleRecordCount == 0)
            {
                ImGui::PushStyleColor(ImGuiCol_Text, EditorColor::TextMuted);
                ImGui::TextWrapped("No matching %s assets.", std::string(Project::ToString(options.Type)).c_str());
                ImGui::PopStyleColor();
            }
        }

        ImGui::EndChild();
        ImGui::EndCombo();
        return changed;
    }
}
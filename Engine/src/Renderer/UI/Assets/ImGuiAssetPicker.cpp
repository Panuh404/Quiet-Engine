#include "Renderer/UI/Assets/ImGuiAssetPicker.h"

#include <algorithm>
#include <array>
#include <string>
#include <vector>

#include <imgui.h>

#include "Quiet/Project/AssetRecord.h"
#include "Quiet/Project/AssetRegistry.h"
#include "Quiet/Project/ProjectContext.h"
#include "Renderer/UI/Assets/ImGuiAssetBrowserUtils.h"
#include "Renderer/UI/Assets/ImGuiAssetBrowserWidgets.h"
#include "Renderer/UI/ImGuiTexturePreview.h"
#include "Renderer/UI/ImGuiTheme.h"
#include "Renderer/Vulkan/Mesh/MeshLibrary.h"
#include "Renderer/Vulkan/Texture/TextureLibrary.h"

namespace
{
    namespace QUI = Quiet::Renderer::UI;
    namespace SZ = Quiet::Renderer::UI::EditorSizing;
    namespace Assets = Quiet::Renderer::UI::Assets;

    std::array<char, 128> g_AssetPickerSearchBuffer{};
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

    void DrawAssetPickerFallback(ImDrawList* drawList, const ImVec2& min, const ImVec2& max, const char* label, const ImVec4& color)
    {
        const ImVec2 textSize = ImGui::CalcTextSize(label);
        const ImVec2 textPosition(min.x + ((max.x - min.x) - textSize.x) * 0.5f, min.y + ((max.y - min.y) - textSize.y) * 0.5f);

        drawList->AddText(
            textPosition,
            ImGui::ColorConvertFloat4ToU32(color),
            label
        );
    }

    std::string MakePickerStatusLabel(const Quiet::Project::AssetRecord& record,
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

    bool IsAssetPickerRecordAssignable(const Quiet::Project::AssetRecord& record,
                                       const Assets::AssetPathDiagnostics& diagnostics,
                                       const Assets::AssetPickerOptions& options)
    {
        if (record.Type != options.Type) return false;
        if (diagnostics.IsMissing()) return false;
        if (record.Type == Quiet::Project::AssetType::Mesh && options.MeshLibrary && !options.MeshLibrary->TryGet(record.ID)) return false;
        return true;
    }

    std::string MakeAssetPickerDisabledReason(const Quiet::Project::AssetRecord& record,
                                              const Assets::AssetPathDiagnostics& diagnostics,
                                              const Assets::AssetPickerOptions& options)
    {
        if (record.Type != options.Type)
            return "This field only accepts " + std::string(Quiet::Project::ToString(options.Type)) + " assets.";

        if (diagnostics.IsMissing())
            return Assets::MakeAssetPathDiagnosticDescription(diagnostics);

        if (record.Type == Quiet::Project::AssetType::Mesh && options.MeshLibrary && !options.MeshLibrary->TryGet(record.ID))
            return "This mesh is registered but not loaded. Reload project assets before assigning it.";

        return {};
    }

    bool DrawAssetPickerRow(const Quiet::Project::AssetRecord& record,
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
                const char* label = record.Type == Quiet::Project::AssetType::Audio ? "AUD" : "TEX";
                DrawAssetPickerFallback(drawList, iconMin, iconMax, label, assignable ? QUI::EditorColor::AccentHover : QUI::EditorColor::TextDim);
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

    void DrawAssetPickerDropOverlay(const ImVec2& fieldMin, const ImVec2& fieldMax,  const std::string& label, const bool assignable)
    {
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        ImVec4 backgroundColor = QUI::EditorColor::Background2;

        backgroundColor.w = 0.96f;
        const ImVec4 borderColor = assignable ? QUI::EditorColor::Success : QUI::EditorColor::Error;

        drawList->AddRectFilled(
            fieldMin,
            fieldMax,
            ImGui::ColorConvertFloat4ToU32(backgroundColor),
            ImGui::GetStyle().FrameRounding
        );

        drawList->AddRect(
            fieldMin,
            fieldMax,
            ImGui::ColorConvertFloat4ToU32(borderColor),
            ImGui::GetStyle().FrameRounding,
            0,
            SZ::Px(2.0f)
        );

        const float horizontalPadding = SZ::Px(12.0f);
        const float availableWidth = std::max(SZ::Px(20.0f), fieldMax.x - fieldMin.x - horizontalPadding * 2.0f);

        const std::string visibleLabel = Assets::MakeEllipsizedText(label, availableWidth);
        const ImVec2 textSize = ImGui::CalcTextSize(visibleLabel.c_str());
        const ImVec2 textPosition(fieldMin.x + (fieldMax.x - fieldMin.x - textSize.x) * 0.5f, fieldMin.y + (fieldMax.y - fieldMin.y - textSize.y) * 0.5f);

        drawList->AddText(
            textPosition,
            ImGui::ColorConvertFloat4ToU32(
                borderColor
            ),
            visibleLabel.c_str()
        );
    }

    bool DrawAssetPickerDropTarget(const Quiet::Project::ProjectContext& projectContext,
                                   const Assets::AssetPickerOptions& options,
                                   const ImVec2& fieldMin,
                                   const ImVec2& fieldMax,
                                   std::string& outSelectedAssetID)
    {
        Assets::AssetBrowserDragPayload payload{};

        const ImGuiDragDropFlags dropFlags = ImGuiDragDropFlags_AcceptBeforeDelivery | ImGuiDragDropFlags_AcceptNoDrawDefaultRect;

        if (!Assets::ReadAssetBrowserDrop(payload, dropFlags)) return false;
        const Quiet::Project::AssetRecord* record = Quiet::Project::AssetRegistryIO::FindByID(projectContext.GetAssetRegistry(), payload.AssetID);

        bool assignable = false;
        std::string overlayLabel;

        if (!record)
        {
            overlayLabel = "Asset is not registered";
        }
        else
        {
            const Assets::AssetPathDiagnostics diagnostics = Assets::InspectAssetPath(projectContext, *record);
            assignable = IsAssetPickerRecordAssignable(*record, diagnostics, options);
            if (assignable)
            {
                overlayLabel = "Assign " + record->ID;
            }
            else if (record->Type != options.Type)
            {
                overlayLabel = std::string(Quiet::Project::ToString(options.Type)) + " asset required";
            }
            else
            {
                overlayLabel = MakePickerStatusLabel(*record, diagnostics, options);
            }
        }

        DrawAssetPickerDropOverlay(fieldMin, fieldMax, overlayLabel, assignable);
        if (!payload.Delivered || !assignable || !record) return false;
        outSelectedAssetID = record->ID;
        return true;
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
        const bool hasCurrentSelection =
    !currentAssetID.empty();

    const Project::AssetRecord* currentRecord = hasCurrentSelection ? Project::AssetRegistryIO::FindByID(projectContext.GetAssetRegistry(), currentAssetID) : nullptr;
    const bool currentRecordMissing = hasCurrentSelection && !currentRecord;
    const bool currentTypeMismatch = currentRecord && currentRecord->Type != options.Type;

    AssetPathDiagnostics currentDiagnostics{};
    if (currentRecord && !currentTypeMismatch)
    {
        currentDiagnostics = InspectAssetPath(projectContext, *currentRecord);
    }

    const bool currentSourceMissing = currentRecord && !currentTypeMismatch && currentDiagnostics.IsMissing();
    const bool currentMeshNotLoaded = currentRecord && !currentTypeMismatch &&
        currentRecord->Type == Project::AssetType::Mesh && options.MeshLibrary && !options.MeshLibrary->TryGet(currentRecord->ID);

    std::string previewLabel = "None";
    if (hasCurrentSelection)
    {
        if (currentRecordMissing)
        {
            previewLabel = "Missing: " + currentAssetID;
        }
        else if (currentTypeMismatch)
        {
            previewLabel = "Invalid Type: " + currentAssetID;
        }
        else if (currentSourceMissing)
        {
            previewLabel = MakeAssetPathDiagnosticLabel(currentDiagnostics) + ": " + currentAssetID;
        }
        else if (currentMeshNotLoaded)
        {
            previewLabel = "Not Loaded: " + currentAssetID;
        }
        else
        {
            previewLabel = currentAssetID;
        }
    }

    const bool currentSelectionError = currentRecordMissing || currentTypeMismatch || currentSourceMissing;
    const bool currentSelectionWarning = !currentSelectionError && currentMeshNotLoaded;

    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    ImGui::SetNextWindowSizeConstraints(SZ::Px(ImVec2(380.0f, 220.0f)), SZ::Px(ImVec2(560.0f, 460.0f)));

    if (currentSelectionError)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, EditorColor::Error);
    }
    else if (currentSelectionWarning)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, EditorColor::Warning);
    }

    const bool pickerOpen = EditorBeginCombo(id, previewLabel.c_str(), ImGuiComboFlags_HeightLargest);

    if (currentSelectionError || currentSelectionWarning)
    {
        ImGui::PopStyleColor();
    }

    if (!pickerOpen)
    {
        const ImVec2 fieldMin = ImGui::GetItemRectMin();
        const ImVec2 fieldMax = ImGui::GetItemRectMax();

        return DrawAssetPickerDropTarget(projectContext, options, fieldMin, fieldMax, outSelectedAssetID);
    }
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

        if (currentRecordMissing)
        {
            ImGui::PushStyleColor(ImGuiCol_Text, EditorColor::Error);
            ImGui::TextWrapped("The currently assigned asset '%s' is no longer present in the registry.", currentAssetID.c_str());
            ImGui::PopStyleColor();

            ImGui::Spacing();
        }
        else if (currentTypeMismatch)
        {
            const std::string assignedType = std::string(Project::ToString(currentRecord->Type));
            const std::string requiredType = std::string(Project::ToString(options.Type));

            ImGui::PushStyleColor(ImGuiCol_Text, EditorColor::Error);
            ImGui::TextWrapped("The assigned asset is type %s, but this field requires a %s asset.", assignedType.c_str(), requiredType.c_str());
            ImGui::PopStyleColor();

            ImGui::Spacing();
        }
        else if (currentSourceMissing)
        {
            ImGui::PushStyleColor(ImGuiCol_Text, EditorColor::Error);
            ImGui::TextWrapped("%s", MakeAssetPathDiagnosticDescription(currentDiagnostics).c_str());
            ImGui::PopStyleColor();

            ImGui::Spacing();
        }
        else if (currentMeshNotLoaded)
        {
            ImGui::PushStyleColor(ImGuiCol_Text, EditorColor::Warning);
            ImGui::TextWrapped("The assigned mesh is registered but not loaded. Reload project assets before using it.");
            ImGui::PopStyleColor();

            ImGui::Spacing();
        }

        std::vector<const Project::AssetRecord*> records = Project::AssetRegistryIO::GetAssetsByType(projectContext.GetAssetRegistry(), options.Type);
        std::sort(
            records.begin(),
            records.end(),
            [](const Project::AssetRecord* left, const Project::AssetRecord* right)
            {
                if (!left) return false;
                if (!right) return true;
                return ToLowerCopy(left->ID) < ToLowerCopy(right->ID);
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
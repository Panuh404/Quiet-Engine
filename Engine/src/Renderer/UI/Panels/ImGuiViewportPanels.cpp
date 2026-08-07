#include "Renderer/UI/ImGuiLayer.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <string>

#include <imgui.h>
#include <imgui_internal.h>

#include "Quiet/Editor/EditorState.h"
#include "Quiet/Project/AssetRecord.h"
#include "Quiet/Project/AssetRegistry.h"
#include "Quiet/Project/ProjectContext.h"
#include "Renderer/UI/Assets/ImGuiAssetBrowserUtils.h"
#include "Renderer/UI/Assets/ImGuiAssetBrowserWidgets.h"
#include "Renderer/UI/ImGuiTheme.h"
#include "Renderer/UI/ImGuiToolbarCommon.h"
#include "Renderer/Vulkan/Mesh/MeshLibrary.h"

namespace
{
    namespace QUI = Quiet::Renderer::UI;
    namespace ASSETS = Quiet::Renderer::UI::Assets;

    constexpr float kMinimumViewportCameraSpeed = 0.1f;
    constexpr float kMaximumViewportCameraSpeed = 100.0f;
    constexpr float kViewportCameraWheelMultiplier = 1.25f;

    constexpr float kMinimumTranslateSnap = 0.001f;
    constexpr float kMaximumTranslateSnap = 1000.0f;

    constexpr float kMinimumRotateSnap = 0.1f;
    constexpr float kMaximumRotateSnap = 180.0f;

    constexpr float kMinimumScaleSnap = 0.001f;
    constexpr float kMaximumScaleSnap = 10.0f;


    constexpr std::array kViewportCameraSpeedPresets =
        {
            0.5f,
            1.0f,
            2.5f,
            5.0f,
            10.0f,
            25.0f,
            50.0f,
            100.0f
        };

    struct ViewportNavigationGuideItem
    {
        const char* Key = "";
        const char* Action = "";
        bool Emphasized = false;
    };

    std::string FormatViewportCameraSpeed(const float speed)
    {
        char buffer[32]{};

        if (speed < 10.0f)
        {
            std::snprintf(buffer, sizeof(buffer), "%.1f u/s", speed);
        }
        else
        {
            std::snprintf(buffer, sizeof(buffer), "%.0f u/s", speed);
        }

        return buffer;
    }

    void UpdateViewportCameraSpeedFromWheel(Quiet::Editor::EditorState& editorState)
    {
        if (editorState.Viewport.NavigationMode != Quiet::Editor::ViewportNavigationMode::Fly) return;

        const float wheelDelta = ImGui::GetIO().MouseWheel;
        if (std::abs(wheelDelta) <= 0.001f) return;

        const float multiplier = std::pow(kViewportCameraWheelMultiplier, wheelDelta);
        editorState.Viewport.CameraMoveSpeed = std::clamp(
            editorState.Viewport.CameraMoveSpeed * multiplier,
            kMinimumViewportCameraSpeed,
            kMaximumViewportCameraSpeed);
    }

    void DrawViewportCameraSpeedPopup(Quiet::Editor::EditorState& editorState)
    {
        if (!ImGui::BeginPopup("##ViewportCameraSpeedPopup")) return;

        ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::Text);
        ImGui::TextUnformatted("Camera Speed");
        ImGui::PopStyleColor();

        ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::TextMuted);
        ImGui::TextWrapped("Hold RMB over the Viewport and use the mouse wheel to adjust the speed.");
        ImGui::PopStyleColor();

        ImGui::Spacing();

        ImGui::SetNextItemWidth(QUI::EditorSizing::Px(220.0f));

        float speed = editorState.Viewport.CameraMoveSpeed;

        if (ImGui::SliderFloat(
                "##ViewportCameraMoveSpeed",
                &speed,
                kMinimumViewportCameraSpeed,
                kMaximumViewportCameraSpeed,
                "%.1f u/s",
                ImGuiSliderFlags_Logarithmic))
        {
            editorState.Viewport.CameraMoveSpeed = std::clamp(
                speed,
                kMinimumViewportCameraSpeed,
                kMaximumViewportCameraSpeed);
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::TextDim);
        ImGui::TextUnformatted("PRESETS");
        ImGui::PopStyleColor();

        for (const float preset : kViewportCameraSpeedPresets)
        {
            const std::string label = FormatViewportCameraSpeed(preset);

            const bool selected = std::abs(editorState.Viewport.CameraMoveSpeed - preset) <= 0.001f;
            if (ImGui::Selectable(label.c_str(), selected))
            {
                editorState.Viewport.CameraMoveSpeed = preset;
            }
        }

        ImGui::EndPopup();
    }

    void DrawViewportSnapSettingsPopup(Quiet::Editor::EditorState& editorState)
    {
        if (!ImGui::BeginPopup("##ViewportSnapSettingsPopup"))
            return;

        auto& gizmo = editorState.Gizmo;
        constexpr float defaultTranslateSnap[3] =
        {
            0.5f,
            0.5f,
            0.5f
        };

        for (std::size_t axis = 0; axis < 3; ++axis)
        {
            if (!std::isfinite(gizmo.TranslateSnap[axis]))
            {
                gizmo.TranslateSnap[axis] = defaultTranslateSnap[axis];
            }

            gizmo.TranslateSnap[axis] = std::clamp(gizmo.TranslateSnap[axis], kMinimumTranslateSnap, kMaximumTranslateSnap);
        }

        if (!std::isfinite(gizmo.RotateSnap))
            gizmo.RotateSnap = 15.0f;

        gizmo.RotateSnap = std::clamp(gizmo.RotateSnap, kMinimumRotateSnap, kMaximumRotateSnap);

        if (!std::isfinite(gizmo.ScaleSnap))
            gizmo.ScaleSnap = 0.1f;

        gizmo.ScaleSnap = std::clamp(gizmo.ScaleSnap, kMinimumScaleSnap, kMaximumScaleSnap);

        ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::Text);
        ImGui::TextUnformatted("Gizmo Snapping");
        ImGui::PopStyleColor();

        ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::TextMuted);
        ImGui::TextWrapped("Configure the increments used by the active Transform gizmo when snapping is enabled.");
        ImGui::PopStyleColor();

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::Checkbox("Enable Snapping", &gizmo.SnapEnabled);

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::TextDim);
        ImGui::TextUnformatted("TRANSLATION");
        ImGui::PopStyleColor();

        ImGui::SetNextItemWidth(QUI::EditorSizing::Px(250.0f));

        ImGui::DragFloat3(
            "##ViewportTranslateSnap",
            gizmo.TranslateSnap,
            0.01f,
            kMinimumTranslateSnap,
            kMaximumTranslateSnap,
            "%.3f",
            ImGuiSliderFlags_AlwaysClamp
        );

        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
        {
            ImGui::SetTooltip("X, Y, and Z translation increments in world units.");
        }

        ImGui::Spacing();

        ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::TextDim);
        ImGui::TextUnformatted("ROTATION");
        ImGui::PopStyleColor();

        ImGui::SetNextItemWidth(QUI::EditorSizing::Px(250.0f));

        ImGui::DragFloat(
            "##ViewportRotateSnap",
            &gizmo.RotateSnap,
            0.5f,
            kMinimumRotateSnap,
            kMaximumRotateSnap,
            "%.1f deg",
            ImGuiSliderFlags_AlwaysClamp
        );

        ImGui::Spacing();

        ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::TextDim);
        ImGui::TextUnformatted("SCALE");
        ImGui::PopStyleColor();

        ImGui::SetNextItemWidth(QUI::EditorSizing::Px(250.0f));

        ImGui::DragFloat(
            "##ViewportScaleSnap",
            &gizmo.ScaleSnap,
            0.01f,
            kMinimumScaleSnap,
            kMaximumScaleSnap,
            "%.3f",
            ImGuiSliderFlags_AlwaysClamp
        );

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (QUI::EditorButton("Restore Default Increments", ImVec2(QUI::EditorSizing::Px(250.0f), 0.0f)))
        {
            gizmo.TranslateSnap[0] = 0.5f;
            gizmo.TranslateSnap[1] = 0.5f;
            gizmo.TranslateSnap[2] = 0.5f;

            gizmo.RotateSnap = 15.0f;
            gizmo.ScaleSnap = 0.1f;
        }

        ImGui::EndPopup();
    }

    void DrawViewportDisplayOptionsPopup(Quiet::Editor::EditorState& editorState)
    {
        if (!ImGui::BeginPopup("##ViewportDisplayOptionsPopup")) return;

        ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::Text);
        ImGui::TextUnformatted("Viewport Display");
        ImGui::PopStyleColor();

        ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::TextMuted);
        ImGui::TextWrapped("Control editor-only Viewport overlays.");
        ImGui::PopStyleColor();

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::Checkbox("Grid", &editorState.Viewport.ShowGrid);
        ImGui::Checkbox("Editor Icons", &editorState.Viewport.ShowDebugIcons);
        ImGui::Checkbox("Selection Feedback", &editorState.Viewport.ShowSelectionFeedback);
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
        {
            ImGui::SetTooltip("Show the selected-mesh silhouette, actor origin and name, and selected Camera or Light feedback.");
        }
        ImGui::Checkbox("Hover Feedback", &editorState.Viewport.ShowHoverFeedback);
        ImGui::Checkbox("Navigation Guide", &editorState.Viewport.ShowNavigationHints);

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (QUI::EditorButton("Restore Defaults", ImVec2(QUI::EditorSizing::Px(190.0f), 0.0f)))
        {
            editorState.Viewport.ShowGrid = true;
            editorState.Viewport.ShowDebugIcons = true;
            editorState.Viewport.ShowSelectionFeedback = true;
            editorState.Viewport.ShowHoverFeedback = true;
            editorState.Viewport.ShowNavigationHints = true;
        }

        ImGui::EndPopup();
    }

    const char* GetViewportModeLabel(const Quiet::Editor::SceneMode mode)
    {
        switch (mode)
        {
            case Quiet::Editor::SceneMode::Edit:     return "Edit";
            case Quiet::Editor::SceneMode::Simulate: return "Simulate";
            case Quiet::Editor::SceneMode::Play:     return "Play";
            default:                                 return "Unknown";
        }
    }

    ImVec4 GetViewportModeColor(const Quiet::Editor::SceneMode mode)
    {
        switch (mode)
        {
            case Quiet::Editor::SceneMode::Edit:
                return QUI::EditorColor::TextMuted;

            case Quiet::Editor::SceneMode::Simulate:
                return QUI::EditorColor::Success;

            case Quiet::Editor::SceneMode::Play:
                return QUI::EditorColor::AccentHover;

            default:
                return QUI::EditorColor::TextDim;
        }
    }

    struct ViewportAssetDropVisual
    {
        bool Visible = false;
        bool Accepted = false;

        std::string Label{};
    };

    ViewportAssetDropVisual HandleViewportMeshAssetDrop(Quiet::Editor::EditorState& editorState,
                                                        const Quiet::Project::ProjectContext& projectContext,
                                                        const Quiet::Renderer::Vulkan::MeshLibrary& meshLibrary)
    {
        ViewportAssetDropVisual visual{};
        ASSETS::AssetBrowserDragPayload payload{};

        const ImGuiDragDropFlags dropFlags = ImGuiDragDropFlags_AcceptBeforeDelivery | ImGuiDragDropFlags_AcceptNoDrawDefaultRect;

        if (!ASSETS::ReadAssetBrowserDrop(payload, dropFlags)) return visual;
        visual.Visible = true;

        if (editorState.Scene.Mode != Quiet::Editor::SceneMode::Edit)
        {
            visual.Label = "Viewport asset drops are disabled during runtime";
            return visual;
        }

        const Quiet::Project::AssetRecord* record = Quiet::Project::AssetRegistryIO::FindByID(projectContext.GetAssetRegistry(), payload.AssetID);
        if (!record)
        {
            visual.Label = "Asset is not registered";
            return visual;
        }

        if (record->Type != Quiet::Project::AssetType::Mesh)
        {
            visual.Label = "Mesh asset required";
            return visual;
        }

        const ASSETS::AssetPathDiagnostics diagnostics = ASSETS::InspectAssetPath(projectContext, *record);
        if (diagnostics.IsMissing())
        {
            visual.Label = ASSETS::MakeAssetPathDiagnosticLabel(diagnostics);
            return visual;
        }

        if (!meshLibrary.TryGet(record->ID))
        {
            visual.Label = "Mesh is registered but not loaded";
            return visual;
        }

        visual.Accepted = true;
        visual.Label = "Release to create " + record->ID + " actor";

        const ImVec2 mousePosition = ImGui::GetMousePos();
        const Quiet::Editor::EditorViewportAssetDropRequest dropRequest{ record->ID, mousePosition.x, mousePosition.y };

        editorState.Viewport.ActiveMeshAssetDrop = dropRequest;
        if (payload.Delivered)
        {
            editorState.Viewport.PendingMeshAssetDrop = dropRequest;
        }

        return visual;
    }

    void DrawViewportAssetDropOverlay(const ViewportAssetDropVisual& visual, const ImVec2& imageMin, const ImVec2& imageSize, const float uiScale)
    {
        if (!visual.Visible || imageSize.x <= 1.0f || imageSize.y <= 1.0f) return;

        const ImVec2 imageMax(imageMin.x + imageSize.x, imageMin.y + imageSize.y);
        const ImVec4 accentColor = visual.Accepted ? QUI::EditorColor::Success : QUI::EditorColor::Error;

        ImVec4 tintColor = accentColor;
        tintColor.w = visual.Accepted ? 0.025f : 0.08f;

        ImDrawList* drawList = ImGui::GetWindowDrawList();

        drawList->PushClipRect(imageMin, imageMax, true);
        drawList->AddRectFilled(
            imageMin,
            imageMax,
            ImGui::ColorConvertFloat4ToU32(tintColor)
        );

        drawList->AddRect(
            ImVec2(imageMin.x + 2.0f * uiScale, imageMin.y + 2.0f * uiScale),
            ImVec2(imageMax.x - 2.0f * uiScale, imageMax.y - 2.0f * uiScale),
            ImGui::ColorConvertFloat4ToU32(accentColor),
            8.0f * uiScale,
            0,
            3.0f * uiScale
        );

        const float horizontalPadding = 18.0f * uiScale;
        const float verticalPadding = 12.0f * uiScale;

        const float maximumCardWidth = std::max(140.0f * uiScale, imageSize.x - 32.0f * uiScale);
        const float desiredCardWidth = ImGui::CalcTextSize(visual.Label.c_str()).x + horizontalPadding * 2.0f;

        const float cardWidth = std::min(std::max(220.0f * uiScale, desiredCardWidth), maximumCardWidth);

        const std::string visibleLabel = ASSETS::MakeEllipsizedText(visual.Label, std::max(30.0f * uiScale, cardWidth - horizontalPadding * 2.0f));

        const ImVec2 textSize = ImGui::CalcTextSize(visibleLabel.c_str());
        const ImVec2 cardSize(cardWidth, textSize.y + verticalPadding * 2.0f);

        const ImVec2 cardMin(imageMin.x + (imageSize.x - cardSize.x) * 0.5f, imageMin.y + (imageSize.y - cardSize.y) * 0.5f);
        const ImVec2 cardMax(cardMin.x + cardSize.x, cardMin.y + cardSize.y);

        ImVec4 backgroundColor = QUI::EditorColor::Background0;
        backgroundColor.w = 0.94f;

        drawList->AddRectFilled(
            cardMin,
            cardMax,
            ImGui::ColorConvertFloat4ToU32(backgroundColor),
            9.0f * uiScale
        );

        drawList->AddRect(
            cardMin,
            cardMax,
            ImGui::ColorConvertFloat4ToU32(accentColor),
            9.0f * uiScale,
            0,
            1.5f * uiScale
        );

        drawList->AddText(
            ImVec2(cardMin.x + (cardSize.x - textSize.x) * 0.5f, cardMin.y + verticalPadding),
            ImGui::ColorConvertFloat4ToU32(accentColor),
            visibleLabel.c_str()
        );

        drawList->PopClipRect();
    }

    void DrawViewportUnavailableState(const float uiScale)
    {
        const ImVec2 available = ImGui::GetContentRegionAvail();

        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 12.0f * uiScale);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(18.0f * uiScale, 16.0f * uiScale));

        ImGui::PushStyleColor(ImGuiCol_ChildBg, QUI::EditorColor::Background2);
        ImGui::PushStyleColor(ImGuiCol_Border, QUI::EditorColor::BorderSoft);

        const ImVec2 cardSize(available.x > 420.0f * uiScale ? 420.0f * uiScale : available.x, 104.0f * uiScale);

        ImGui::SetCursorPos(ImVec2((available.x - cardSize.x) * 0.5f, (available.y - cardSize.y) * 0.5f ));

        ImGui::BeginChild("##ViewportUnavailableCard", cardSize, ImGuiChildFlags_Borders);
        {
            ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::Text);
            ImGui::TextUnformatted("Scene viewport unavailable");
            ImGui::PopStyleColor();

            ImGui::Spacing();

            ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::TextMuted);
            ImGui::TextWrapped("The scene render target is not ready yet. Resize the viewport or reload the renderer if this persists.");
            ImGui::PopStyleColor();
        }
        ImGui::EndChild();

        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(2);
    }

    void DrawViewportStatusOverlay(const Quiet::Editor::EditorState& editorState, const ImVec2& imageMin, const ImVec2& imageSize, const float uiScale)
    {
        if (imageSize.x < 220.0f || imageSize.y < 120.0f)
            return;

        char resolutionText[64]{};
        std::snprintf(
            resolutionText,
            sizeof(resolutionText),
            "%u x %u",
            editorState.Viewport.Width,
            editorState.Viewport.Height
        );

        std::string statusText = GetViewportModeLabel(editorState.Scene.Mode);
        statusText += "  |  ";
        statusText += resolutionText;

        if (editorState.Gizmo.SnapEnabled)
            statusText += "  |  Snap";

        if (editorState.Viewport.ShowGrid && editorState.Scene.Mode != Quiet::Editor::SceneMode::Play)
        {
            statusText += "  |  Grid";
        }

        if (editorState.Viewport.ShowDebugIcons)
            statusText += "  |  Icons";

        const auto navigationMode = editorState.Viewport.NavigationMode;
        if (navigationMode != Quiet::Editor::ViewportNavigationMode::None)
        {
            statusText += "  |  ";

            const bool pilotingCamera =
                editorState.Scene.Mode == Quiet::Editor::SceneMode::Edit &&
                editorState.Selection.PilotedCameraActorID.has_value();

            switch (navigationMode)
            {
                case Quiet::Editor::ViewportNavigationMode::Fly:
                {
                    const float effectiveSpeed = editorState.Viewport.CameraMoveSpeed * (editorState.Viewport.FlySpeedBoostActive ? 2.0f : 1.0f);
                    statusText += pilotingCamera ? "Pilot " : "Fly ";
                    statusText += FormatViewportCameraSpeed(effectiveSpeed);

                    if (editorState.Viewport.FlySpeedBoostActive)
                    {
                        statusText += "  Boost";
                    }
                    break;
                }

                case Quiet::Editor::ViewportNavigationMode::Orbit:
                    statusText += pilotingCamera ? "Pilot Orbit" : "Orbit";
                    break;

                case Quiet::Editor::ViewportNavigationMode::Pan:
                    statusText += pilotingCamera ? "Pilot Pan" : "Pan";
                    break;

                case Quiet::Editor::ViewportNavigationMode::None:
                default:
                    break;
            }
        }

        const ImVec2 padding = ImVec2(9.0f * uiScale, 6.0f * uiScale);
        const ImVec2 textSize = ImGui::CalcTextSize(statusText.c_str());

        const ImVec2 panelSize(textSize.x + padding.x * 2.0f, textSize.y + padding.y * 2.0f);
        const ImVec2 panelMin(imageMin.x + 12.0f * uiScale, imageMin.y + imageSize.y - panelSize.y - 12.0f * uiScale);
        const ImVec2 panelMax(panelMin.x + panelSize.x, panelMin.y + panelSize.y);

        ImDrawList* drawList = ImGui::GetWindowDrawList();

        drawList->AddRectFilled(
            panelMin,
            panelMax,
            ImGui::ColorConvertFloat4ToU32(ImVec4(
                QUI::EditorColor::Background0.x,
                QUI::EditorColor::Background0.y,
                QUI::EditorColor::Background0.z,
                0.86f
            )),
            8.0f * uiScale
        );

        drawList->AddRect(
            panelMin,
            panelMax,
            ImGui::ColorConvertFloat4ToU32(QUI::EditorColor::BorderSoft),
            8.0f * uiScale
        );

        drawList->AddText(
            ImVec2(panelMin.x + padding.x, panelMin.y + padding.y),
            ImGui::ColorConvertFloat4ToU32(GetViewportModeColor(editorState.Scene.Mode)),
            statusText.c_str()
        );
    }

    float MeasureViewportNavigationGuideItem(const ViewportNavigationGuideItem& item, const float uiScale)
    {
        const float keyPaddingX = 7.0f * uiScale;
        const float keyActionGap = 6.0f * uiScale;

        const ImVec2 keyTextSize = ImGui::CalcTextSize(item.Key);
        const ImVec2 actionTextSize = ImGui::CalcTextSize(item.Action);

        return keyTextSize.x + keyPaddingX * 2.0f + keyActionGap + actionTextSize.x;
    }

    float MeasureViewportNavigationGuideRow(const std::array<ViewportNavigationGuideItem, 5>& items, const std::size_t itemCount, const float uiScale)
    {
        const float itemSpacing = 16.0f * uiScale;
        float width = 0.0f;

        for (std::size_t index = 0; index < itemCount; ++index)
        {
            if (index > 0) width += itemSpacing;
            width += MeasureViewportNavigationGuideItem(items[index], uiScale);
        }

        return width;
    }

    void DrawViewportNavigationGuideRow(ImDrawList* drawList, const ImVec2& startPosition, const std::array<ViewportNavigationGuideItem, 5>& items, const std::size_t itemCount, const float uiScale)
    {
        if (!drawList || itemCount == 0) return;

        const float keyPaddingX = 7.0f * uiScale;
        const float keyPaddingY = 3.0f * uiScale;
        const float keyActionGap = 6.0f * uiScale;
        const float itemSpacing = 16.0f * uiScale;
        const float rounding = 4.0f * uiScale;

        ImVec2 cursor = startPosition;

        for (std::size_t index = 0; index < itemCount; ++index)
        {
           const ViewportNavigationGuideItem& item = items[index];

            if (index > 0) cursor.x += itemSpacing;

            const ImVec2 keyTextSize = ImGui::CalcTextSize(item.Key);
            const ImVec2 actionTextSize = ImGui::CalcTextSize(item.Action);

            const ImVec2 keySize(keyTextSize.x + keyPaddingX * 2.0f, keyTextSize.y + keyPaddingY * 2.0f);
            const ImVec2 keyMin = cursor;
            const ImVec2 keyMax(keyMin.x + keySize.x, keyMin.y + keySize.y);

            ImVec4 keyBackground = item.Emphasized
                ? QUI::EditorColor::AccentDim
                : QUI::EditorColor::Background3;

            ImVec4 keyBorder = item.Emphasized
                ? QUI::EditorColor::AccentHover
                : QUI::EditorColor::Border;

            ImVec4 keyTextColor = item.Emphasized
                ? QUI::EditorColor::AccentHover
                : QUI::EditorColor::Text;

            drawList->AddRectFilled(
                keyMin,
                keyMax,
                ImGui::ColorConvertFloat4ToU32(keyBackground),
                rounding
            );

            drawList->AddRect(
                keyMin,
                keyMax,
                ImGui::ColorConvertFloat4ToU32(keyBorder),
                rounding,
                0,
                std::max(1.0f, uiScale)
            );

            drawList->AddText(
                ImVec2(
                    keyMin.x + (keySize.x - keyTextSize.x) * 0.5f,
                    keyMin.y + (keySize.y - keyTextSize.y) * 0.5f
                ),
                ImGui::ColorConvertFloat4ToU32(keyTextColor),
                item.Key
            );

            const ImVec2 actionPosition(
                keyMax.x + keyActionGap,
                keyMin.y + (keySize.y - actionTextSize.y) * 0.5f
            );

            drawList->AddText(
                actionPosition,
                ImGui::ColorConvertFloat4ToU32(QUI::EditorColor::TextMuted),
                item.Action
            );

            cursor.x = actionPosition.x + actionTextSize.x;
        }
    }

    void DrawViewportNavigationGuideOverlay(const Quiet::Editor::EditorState& editorState, const ImVec2& imageMin, const ImVec2& imageSize, const float uiScale)
    {
        if (!editorState.Viewport.ShowNavigationHints) return;
        if (editorState.Scene.Mode == Quiet::Editor::SceneMode::Play) return;

        const Quiet::Editor::ViewportNavigationMode navigationMode = editorState.Viewport.NavigationMode;
        const bool navigationActive = navigationMode != Quiet::Editor::ViewportNavigationMode::None;

        // The idle guide is contextual rather than permanently
        // occupying Viewport space.
        if (!navigationActive && !editorState.Viewport.ImageHovered) return;
        if (imageSize.x < 360.0f * uiScale ||
            imageSize.y < 110.0f * uiScale) return;

        const bool pilotingCamera =
            editorState.Scene.Mode == Quiet::Editor::SceneMode::Edit &&
            editorState.Selection.PilotedCameraActorID.has_value();

        std::string title;

        std::array<ViewportNavigationGuideItem, 5> primaryItems{};
        std::array<ViewportNavigationGuideItem, 5> secondaryItems{};
        std::size_t primaryItemCount = 0;
        std::size_t secondaryItemCount = 0;

        switch (navigationMode)
        {
            case Quiet::Editor::ViewportNavigationMode::Fly:
            {
                title = pilotingCamera ? "PILOT FLY CONTROLS" : "FLY CONTROLS";
                primaryItems[primaryItemCount++] =
                    {
                        "WASD",
                        "Move"
                    };
                primaryItems[primaryItemCount++] =
                    {
                        "Q / E",
                        "Vertical"
                    };
                secondaryItems[secondaryItemCount++] =
                    {
                        "Wheel",
                        "Speed"
                    };
                secondaryItems[secondaryItemCount++] =
                    {
                        "Shift",
                        "Boost x2",
                        editorState.Viewport.FlySpeedBoostActive
                    };
                secondaryItems[secondaryItemCount++] =
                    {
                        "RMB",
                        "Release"
                    };
                break;
            }

            case Quiet::Editor::ViewportNavigationMode::Orbit:
            {
                title = pilotingCamera ? "PILOT ORBIT CONTROLS" : "ORBIT CONTROLS";
                primaryItems[primaryItemCount++] =
                    {
                        "Mouse",
                        "Orbit pivot"
                    };
                primaryItems[primaryItemCount++] =
                    {
                        "Alt+LMB",
                        "Release"
                    };
                break;
            }

            case Quiet::Editor::ViewportNavigationMode::Pan:
            {
                title = pilotingCamera ? "PILOT PAN CONTROLS" : "PAN CONTROLS";
                primaryItems[primaryItemCount++] =
                    {
                        "Mouse",
                        "Pan view"
                    };
                primaryItems[primaryItemCount++] =
                    {
                        "MMB",
                        "Release"
                    };
                break;
            }

            case Quiet::Editor::ViewportNavigationMode::None:
            default:
            {
                title = "VIEWPORT NAVIGATION";
                primaryItems[primaryItemCount++] =
                    {
                        "RMB",
                        "Fly"
                    };
                primaryItems[primaryItemCount++] =
                    {
                        "Alt+LMB",
                        "Orbit"
                    };
                primaryItems[primaryItemCount++] =
                    {
                        "MMB",
                        "Pan"
                    };
                break;
            }
        }

        const float horizontalPadding = 11.0f * uiScale;
        const float verticalPadding = 8.0f * uiScale;
        const float titleGap = 7.0f * uiScale;
        const float rowSpacing = 5.0f * uiScale;

        const float keyRowHeight = ImGui::GetTextLineHeight() + 6.0f * uiScale;
        const ImVec2 titleSize = ImGui::CalcTextSize(title.c_str());

        const float primaryRowWidth = MeasureViewportNavigationGuideRow(primaryItems, primaryItemCount, uiScale);
        const float secondaryRowWidth = MeasureViewportNavigationGuideRow(secondaryItems, secondaryItemCount, uiScale);

        const float contentWidth =
            std::max(
                titleSize.x,
                std::max(primaryRowWidth, secondaryRowWidth)
            );

        float contentHeight = titleSize.y + titleGap + keyRowHeight;
        if (secondaryItemCount > 0)
        {
            contentHeight +=  rowSpacing + keyRowHeight;
        }

        const ImVec2 panelSize(
            contentWidth + horizontalPadding * 2.0f,
            contentHeight + verticalPadding * 2.0f
        );

        const float margin = 12.0f * uiScale;
        if (panelSize.x + margin * 2.0f > imageSize.x ||
            panelSize.y + margin * 2.0f > imageSize.y)
        {
            return;
        }

        const ImVec2 imageMax(
            imageMin.x + imageSize.x,
            imageMin.y + imageSize.y
        );

        const ImVec2 panelMax(
            imageMax.x - margin,
            imageMax.y - margin
        );

        const ImVec2 panelMin(
            panelMax.x - panelSize.x,
            panelMax.y - panelSize.y
        );

        ImVec4 background = QUI::EditorColor::Background0;
        background.w = navigationActive ? 0.90f : 0.76f;

        ImVec4 border = navigationActive
            ? QUI::EditorColor::AccentSoft
            : QUI::EditorColor::BorderSoft;

        const float rounding = 7.0f * uiScale;
        ImDrawList* drawList = ImGui::GetWindowDrawList();

        drawList->PushClipRect(imageMin, imageMax, true);
        drawList->AddRectFilled(
            panelMin,
            panelMax,
            ImGui::ColorConvertFloat4ToU32(background),
            rounding
        );

        drawList->AddRect(
            panelMin,
            panelMax,
            ImGui::ColorConvertFloat4ToU32(border),
            rounding,
            0,
            std::max(1.0f, uiScale)
        );

        if (navigationActive)
        {
            drawList->AddRectFilled(
                panelMin,
                ImVec2(panelMin.x + 3.0f * uiScale, panelMax.y),
                ImGui::ColorConvertFloat4ToU32(QUI::EditorColor::Accent),
                rounding,
                ImDrawFlags_RoundCornersLeft
            );
        }

        const ImVec2 titlePosition(
            panelMin.x + horizontalPadding,
            panelMin.y + verticalPadding
        );

        drawList->AddText(
            titlePosition,
            ImGui::ColorConvertFloat4ToU32(navigationActive
                    ? QUI::EditorColor::AccentHover
                    : QUI::EditorColor::TextDim
            ),
            title.c_str()
        );

        const ImVec2 primaryRowPosition(
            panelMin.x + horizontalPadding,
            titlePosition.y + titleSize.y + titleGap
        );

        DrawViewportNavigationGuideRow(
            drawList,
            primaryRowPosition,
            primaryItems,
            primaryItemCount,
            uiScale
        );

        if (secondaryItemCount > 0)
        {
            const ImVec2 secondaryRowPosition(
                panelMin.x + horizontalPadding,
                primaryRowPosition.y + keyRowHeight + rowSpacing
            );

            DrawViewportNavigationGuideRow(
                drawList,
                secondaryRowPosition,
                secondaryItems,
                secondaryItemCount,
                uiScale
            );
        }

        drawList->PopClipRect();
    }
}

namespace Quiet::Renderer::UI
{
    void ImGuiLayer::BuildRuntimeUI(std::uint32_t& outViewportWidth, std::uint32_t& outViewportHeight, ImTextureID sceneViewportTexture)
    {
        const ImGuiViewport* viewport = ImGui::GetMainViewport();

        ImGuiWindowFlags windowFlags =
            ImGuiWindowFlags_NoDecoration |
            ImGuiWindowFlags_NoDocking |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoNav |
            ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoScrollWithMouse;

        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::SetNextWindowViewport(viewport->ID);

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

        ImGui::Begin("##RuntimeViewportHost", nullptr, windowFlags);
        ImGui::PopStyleVar(3);

        const ImVec2 availableSize = ImGui::GetContentRegionAvail();

        outViewportWidth = availableSize.x > 1.0f ? static_cast<std::uint32_t>(availableSize.x) : 0;
        outViewportHeight = availableSize.y > 1.0f ? static_cast<std::uint32_t>(availableSize.y) : 0;

        if (sceneViewportTexture && outViewportWidth > 0 && outViewportHeight > 0)
        {
            ImGui::Image(
                sceneViewportTexture,
                ImVec2(
                    static_cast<float>(outViewportWidth),
                    static_cast<float>(outViewportHeight)
                )
            );
        }
        else
        {
            ImGui::SetCursorPos(Scale(ImVec2(24.0f, 24.0f)));
            ImGui::TextUnformatted("Runtime viewport unavailable.");
        }

        ImGui::End();
    }

    void ImGuiLayer::BuildViewportToolbar(Editor::EditorState& editorState, const ImVec2& imageMin)
    {
        if (editorState.Scene.Mode != Editor::SceneMode::Edit) return;

        const float toolbarOffset = Scale(10.0f);
        const ImVec2 toolbarPos(
            imageMin.x + toolbarOffset,
            imageMin.y + toolbarOffset
        );

        ImGuiWindowFlags flags =
            ImGuiWindowFlags_NoDecoration |
            ImGuiWindowFlags_NoDocking |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoNav |
            ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_AlwaysAutoResize;

        ImGui::SetNextWindowPos(toolbarPos);
        ImGui::SetNextWindowBgAlpha(0.92f);

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, Scale(10.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, Scale(1.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, Scale(ImVec2(8.0f, 8.0f)));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, Scale(ImVec2(6.0f, 6.0f)));

        PushToolbarStyle();

        ImGui::Begin("##ViewportToolbar", nullptr, flags);
        {
            if (DrawToolbarButton(
                    m_Fonts,
                    "ViewportTranslate",
                    Icons::Translate,
                    "T",
                    "Translate (T)",
                    editorState.Gizmo.Operation == Editor::GizmoOperation::Translate))
            {
                editorState.Gizmo.Operation = Editor::GizmoOperation::Translate;
            }

            ImGui::SameLine();

            if (DrawToolbarButton(
                    m_Fonts,
                    "ViewportRotate",
                    Icons::Rotate,
                    "R",
                    "Rotate (R)",
                    editorState.Gizmo.Operation == Editor::GizmoOperation::Rotate))
            {
                editorState.Gizmo.Operation = Editor::GizmoOperation::Rotate;
            }

            ImGui::SameLine();

            if (DrawToolbarButton(
                    m_Fonts,
                    "ViewportScale",
                    Icons::Scale,
                    "S",
                    "Scale (S)",
                    editorState.Gizmo.Operation == Editor::GizmoOperation::Scale))
            {
                editorState.Gizmo.Operation = Editor::GizmoOperation::Scale;
            }

            ImGui::SameLine();

            ImGui::PushStyleColor(ImGuiCol_Separator, EditorColor::BorderSoft);
            ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
            ImGui::PopStyleColor();

            ImGui::SameLine();

            const bool canToggleGizmoMode = editorState.Gizmo.Operation != Editor::GizmoOperation::Scale;
            const bool isLocalMode = editorState.Gizmo.Mode == Editor::GizmoMode::Local;

            if (DrawToolbarButton(
                    m_Fonts,
                    "ViewportGizmoMode",
                    isLocalMode ? Icons::Local : Icons::World,
                    isLocalMode ? "Local" : "World",
                    canToggleGizmoMode
                        ? (isLocalMode ? "Local Space (L)" : "World Space (L)")
                        : "World/Local mode is not used for Scale",
                    isLocalMode,
                    canToggleGizmoMode,
                    ImVec2(m_Fonts.HasIcons
                            ? ToolbarStyle::ButtonSize()
                            : ToolbarStyle::WideButtonWidth(),
                        ToolbarStyle::ButtonHeight())))
            {
                editorState.Gizmo.Mode = isLocalMode ? Editor::GizmoMode::World : Editor::GizmoMode::Local;
            }

            ImGui::SameLine();

            const bool snapButtonPressed =
                DrawToolbarButton(
                    m_Fonts,
                    "ViewportSnap",
                    Icons::Snap,
                    "Snap",
                    "Toggle Snap (G)\n"
                    "Right-click to configure increments.",
                    editorState.Gizmo.SnapEnabled,
                    true,
                    ImVec2(
                        m_Fonts.HasIcons ? ToolbarStyle::ButtonSize() : ToolbarStyle::WideButtonWidth(),
                        ToolbarStyle::ButtonHeight()
                    )
                );

            const bool snapSettingsRequested = ImGui::IsItemClicked(ImGuiMouseButton_Right);
            if (snapButtonPressed)
            {
                editorState.Gizmo.SnapEnabled =!editorState.Gizmo.SnapEnabled;
            }

            if (snapSettingsRequested)
            {
                ImGui::OpenPopup("##ViewportSnapSettingsPopup");
            }

            DrawViewportSnapSettingsPopup(editorState);

            ImGui::SameLine();

            const bool displayPopupOpen = ImGui::IsPopupOpen("##ViewportDisplayOptionsPopup");
            if (DrawToolbarButton(
                    m_Fonts,
                    "ViewportDisplay",
                    Icons::Gear,
                    "Display",
                    "Viewport display options",
                    displayPopupOpen,
                    true,
                    ImVec2(m_Fonts.HasIcons
                            ? ToolbarStyle::ButtonSize()
                            : ToolbarStyle::WideButtonWidth(),
                        ToolbarStyle::ButtonHeight()
                    )))
            {
                ImGui::OpenPopup("##ViewportDisplayOptionsPopup");
            }

            DrawViewportDisplayOptionsPopup(editorState);

            ImGui::SameLine();

            const std::string cameraSpeedLabel = FormatViewportCameraSpeed(editorState.Viewport.CameraMoveSpeed) + "##ViewportCameraSpeed";
            if (EditorButton(cameraSpeedLabel.c_str(), ImVec2(QUI::EditorSizing::Px(88.0f), ToolbarStyle::ButtonHeight())))
            {
                ImGui::OpenPopup("##ViewportCameraSpeedPopup");
            }

            if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
            {
                ImGui::SetTooltip("Camera speed\n Hold RMB and use the mouse wheel to adjust.");
            }

            DrawViewportCameraSpeedPopup(editorState);

            ImGui::SameLine();

            if (DrawToolbarButton(
                    m_Fonts,
                    "ViewportFocus",
                    Icons::Focus,
                    "Focus",
                    "Focus Selected (F)",
                    false,
                    true,
                    ImVec2(
                        m_Fonts.HasIcons
                            ? ToolbarStyle::ButtonSize()
                            : ToolbarStyle::WideButtonWidth(),
                        ToolbarStyle::ButtonHeight())))
            {
                editorState.QueueRequest(Editor::EditorRequestType::FocusSelected);
            }
        }
        ImGui::End();

        PopToolbarStyle();

        ImGui::PopStyleVar(4);
    }

    void ImGuiLayer::BuildViewport(Editor::EditorState& editorState,
                                   const Project::ProjectContext& projectContext,
                                   const Vulkan::MeshLibrary& meshLibrary,
                                   ImTextureID sceneViewportTexture,
                                   const std::function<void()>& viewportOverlayCallback)
    {
        ImGui::Begin("Viewport");
        {
            editorState.Viewport.ActiveMeshAssetDrop.reset();
            editorState.Viewport.Focused = ImGui::IsWindowFocused();
            editorState.Viewport.Hovered = ImGui::IsWindowHovered();

            const ImVec2 availableSize = ImGui::GetContentRegionAvail();

            editorState.Viewport.Width = availableSize.x > 1.0f ? static_cast<std::uint32_t>(availableSize.x) : 0;
            editorState.Viewport.Height = availableSize.y > 1.0f ? static_cast<std::uint32_t>(availableSize.y) : 0;

            if (sceneViewportTexture && editorState.Viewport.Width > 0 && editorState.Viewport.Height > 0)
            {
                const ImVec2 imageMin = ImGui::GetCursorScreenPos();
                const ImVec2 imageSize(static_cast<float>(editorState.Viewport.Width), static_cast<float>(editorState.Viewport.Height));

                ImGui::Image(sceneViewportTexture, imageSize);
                editorState.Viewport.ImageHovered = ImGui::IsItemHovered();
                if (editorState.Viewport.ImageHovered)
                {
                    const bool altDown = ImGui::GetIO().KeyAlt;

                    if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
                    {
                        editorState.Viewport.NavigationCaptureRequested = Editor::ViewportNavigationMode::Fly;
                    }
                    else if (altDown && ImGui::IsItemClicked(ImGuiMouseButton_Left))
                    {
                        editorState.Viewport.NavigationCaptureRequested = Editor::ViewportNavigationMode::Orbit;
                    }
                    else if (ImGui::IsItemClicked(ImGuiMouseButton_Middle))
                    {
                        editorState.Viewport.NavigationCaptureRequested = Editor::ViewportNavigationMode::Pan;
                    }
                }
                UpdateViewportCameraSpeedFromWheel(editorState);
                editorState.Viewport.ScreenX = imageMin.x;
                editorState.Viewport.ScreenY = imageMin.y;

                // This must run immediately after ImGui::Image because the image
                // must remain the active ImGui drag/drop target item.
                const ViewportAssetDropVisual assetDropVisual = HandleViewportMeshAssetDrop(editorState, projectContext, meshLibrary);
                if (viewportOverlayCallback) viewportOverlayCallback();

                BuildViewportToolbar(editorState, imageMin);

                DrawViewportStatusOverlay(editorState, imageMin, imageSize, m_UIScale);
                if (!assetDropVisual.Visible)
                {
                    DrawViewportNavigationGuideOverlay(editorState, imageMin, imageSize, m_UIScale);
                }
                DrawViewportAssetDropOverlay(assetDropVisual, imageMin, imageSize, m_UIScale);
            }
            else
            {
                editorState.Viewport.NavigationCaptureRequested = Editor::ViewportNavigationMode::None;
                editorState.Viewport.ImageHovered = false;
                DrawViewportUnavailableState(m_UIScale);
            }
        }
        ImGui::End();
    }
}


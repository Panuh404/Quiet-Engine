#include "Renderer/UI/ImGuiLayer.h"

#include <cstdio>
#include <string>

#include <imgui.h>
#include <imgui_internal.h>

#include "Quiet/Editor/EditorState.h"
#include "Renderer/UI/ImGuiTheme.h"
#include "Renderer/UI/ImGuiToolbarCommon.h"

namespace
{
    namespace QUI = Quiet::Renderer::UI;

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

    void DrawViewportUnavailableState(const float uiScale)
    {
        const ImVec2 available = ImGui::GetContentRegionAvail();

        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 12.0f * uiScale);
        ImGui::PushStyleVar(
            ImGuiStyleVar_WindowPadding,
            ImVec2(18.0f * uiScale, 16.0f * uiScale)
        );

        ImGui::PushStyleColor(ImGuiCol_ChildBg, QUI::EditorColor::Background2);
        ImGui::PushStyleColor(ImGuiCol_Border, QUI::EditorColor::BorderSoft);

        const ImVec2 cardSize(
            available.x > 420.0f * uiScale ? 420.0f * uiScale : available.x,
            104.0f * uiScale
        );

        ImGui::SetCursorPos(ImVec2(
            (available.x - cardSize.x) * 0.5f,
            (available.y - cardSize.y) * 0.5f
        ));

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

    void DrawViewportStatusOverlay(const Quiet::Editor::EditorState& editorState,
                                   const ImVec2& imageMin,
                                   const ImVec2& imageSize,
                                   const float uiScale)
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

        if (editorState.Viewport.ShowDebugIcons)
            statusText += "  |  Icons";

        const ImVec2 padding = ImVec2(9.0f * uiScale, 6.0f * uiScale);
        const ImVec2 textSize = ImGui::CalcTextSize(statusText.c_str());

        const ImVec2 panelSize(
            textSize.x + padding.x * 2.0f,
            textSize.y + padding.y * 2.0f
        );

        const ImVec2 panelMin(
            imageMin.x + 12.0f * uiScale,
            imageMin.y + imageSize.y - panelSize.y - 12.0f * uiScale
        );

        const ImVec2 panelMax(
            panelMin.x + panelSize.x,
            panelMin.y + panelSize.y
        );

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
}

namespace Quiet::Renderer::UI
{
    void ImGuiLayer::BuildRuntimeUI(std::uint32_t& outViewportWidth,
                                    std::uint32_t& outViewportHeight,
                                    ImTextureID sceneViewportTexture)
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

            const bool canToggleGizmoMode =
                editorState.Gizmo.Operation != Editor::GizmoOperation::Scale;

            const bool isLocalMode =
                editorState.Gizmo.Mode == Editor::GizmoMode::Local;

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
                    ImVec2(
                        m_Fonts.HasIcons
                            ? ToolbarStyle::ButtonSize()
                            : ToolbarStyle::WideButtonWidth(),
                        ToolbarStyle::ButtonHeight())))
            {
                editorState.Gizmo.Mode =
                    isLocalMode
                        ? Editor::GizmoMode::World
                        : Editor::GizmoMode::Local;
            }

            ImGui::SameLine();

            if (DrawToolbarButton(
                    m_Fonts,
                    "ViewportSnap",
                    Icons::Snap,
                    "Snap",
                    "Toggle Snap (G)",
                    editorState.Gizmo.SnapEnabled,
                    true,
                    ImVec2(
                        m_Fonts.HasIcons
                            ? ToolbarStyle::ButtonSize()
                            : ToolbarStyle::WideButtonWidth(),
                        ToolbarStyle::ButtonHeight())))
            {
                editorState.Gizmo.SnapEnabled = !editorState.Gizmo.SnapEnabled;
            }

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

            ImGui::SameLine();

            if (DrawToolbarButton(
                    m_Fonts,
                    "ViewportDebugIcons",
                    Icons::DebugIcons,
                    "Icons",
                    editorState.Viewport.ShowDebugIcons
                        ? "Hide editor icons"
                        : "Show editor icons",
                    editorState.Viewport.ShowDebugIcons,
                    true,
                    ImVec2(
                        m_Fonts.HasIcons
                            ? ToolbarStyle::ButtonSize()
                            : ToolbarStyle::WideButtonWidth(),
                        ToolbarStyle::ButtonHeight())))
            {
                editorState.Viewport.ShowDebugIcons =
                    !editorState.Viewport.ShowDebugIcons;
            }
        }
        ImGui::End();

        PopToolbarStyle();

        ImGui::PopStyleVar(4);
    }

    void ImGuiLayer::BuildViewport(Editor::EditorState& editorState,
                                   ImTextureID sceneViewportTexture,
                                   const std::function<void()>& viewportOverlayCallback)
    {
        ImGui::Begin("Viewport");
        {
            editorState.Viewport.Focused = ImGui::IsWindowFocused();
            editorState.Viewport.Hovered = ImGui::IsWindowHovered();

            const ImVec2 availableSize = ImGui::GetContentRegionAvail();

            editorState.Viewport.Width = availableSize.x > 1.0f ? static_cast<std::uint32_t>(availableSize.x) : 0;
            editorState.Viewport.Height = availableSize.y > 1.0f ? static_cast<std::uint32_t>(availableSize.y) : 0;

            if (sceneViewportTexture && editorState.Viewport.Width > 0 && editorState.Viewport.Height > 0)
            {
                const ImVec2 imageMin = ImGui::GetCursorScreenPos();
                const ImVec2 imageSize(
                    static_cast<float>(editorState.Viewport.Width),
                    static_cast<float>(editorState.Viewport.Height)
                );

                ImGui::Image(sceneViewportTexture, imageSize);

                editorState.Viewport.ImageHovered = ImGui::IsItemHovered();
                editorState.Viewport.ScreenX = imageMin.x;
                editorState.Viewport.ScreenY = imageMin.y;

                BuildViewportToolbar(editorState, imageMin);
                if (viewportOverlayCallback)
                    viewportOverlayCallback();

                DrawViewportStatusOverlay(
                    editorState,
                    imageMin,
                    imageSize,
                    m_UIScale
                );
            }
            else
            {
                editorState.Viewport.ImageHovered = false;
                DrawViewportUnavailableState(m_UIScale);
            }
        }
        ImGui::End();
    }
}


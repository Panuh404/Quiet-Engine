#include "ImGuiLayer.h"

#include <string>
#include <imgui.h>
#include <imgui_internal.h>

#include "Renderer/UI/ImGuiToolbarCommon.h"
#include "Quiet/Editor/EditorState.h"

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
            ImGui::SetCursorPos(ImVec2(24.0f, 24.0f));
            ImGui::TextUnformatted("Runtime viewport unavailable.");
        }

        ImGui::End();
    }

    void ImGuiLayer::BuildViewportToolbar(Editor::EditorState& editorState,
                                          const ImVec2& imageMin)
    {
        if (editorState.CurrentSceneMode != Editor::SceneMode::Edit) return;
        const ImVec2 toolbarPos(imageMin.x + 10.0f, imageMin.y + 10.0f);

        ImGuiWindowFlags flags =
            ImGuiWindowFlags_NoDecoration |
            ImGuiWindowFlags_NoDocking |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoNav |
            ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_AlwaysAutoResize;

        ImGui::SetNextWindowPos(toolbarPos);
        ImGui::SetNextWindowBgAlpha(0.90f);

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 10.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 10.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 8.0f));

        ImGui::Begin("##ViewportToolbar", nullptr, flags);
        {
            if (DrawToolbarButton(
                    m_Fonts,
                    "ViewportTranslate",
                    Icons::Translate,
                    "T",
                    "Translate (T)",
                    editorState.CurrentGizmoOperation == Editor::GizmoOperation::Translate))
            {
                editorState.CurrentGizmoOperation = Editor::GizmoOperation::Translate;
            }

            ImGui::SameLine();

            if (DrawToolbarButton(
                    m_Fonts,
                    "ViewportRotate",
                    Icons::Rotate,
                    "R",
                    "Rotate (R)",
                    editorState.CurrentGizmoOperation == Editor::GizmoOperation::Rotate))
            {
                editorState.CurrentGizmoOperation = Editor::GizmoOperation::Rotate;
            }

            ImGui::SameLine();

            if (DrawToolbarButton(
                    m_Fonts,
                    "ViewportScale",
                    Icons::Scale,
                    "S",
                    "Scale (S)",
                    editorState.CurrentGizmoOperation == Editor::GizmoOperation::Scale))
            {
                editorState.CurrentGizmoOperation = Editor::GizmoOperation::Scale;
            }

            ImGui::SameLine();
            ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
            ImGui::SameLine();

            const bool canToggleGizmoMode = editorState.CurrentGizmoOperation != Editor::GizmoOperation::Scale;
            const bool isLocalMode = editorState.CurrentGizmoMode == Editor::GizmoMode::Local;

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
                    ImVec2(m_Fonts.HasIcons ? ToolbarStyle::ButtonSize : ToolbarStyle::WideButtonWidth, ToolbarStyle::ButtonHeight)))
            {
                editorState.CurrentGizmoMode = isLocalMode ? Editor::GizmoMode::World : Editor::GizmoMode::Local;
            }

            ImGui::SameLine();

            if (DrawToolbarButton(
                    m_Fonts,
                    "ViewportSnap",
                    Icons::Snap,
                    "Snap",
                    "Toggle Snap (G)",
                    editorState.GizmoSnapEnabled,
                    true,
                    ImVec2(m_Fonts.HasIcons ? ToolbarStyle::ButtonSize : ToolbarStyle::WideButtonWidth, ToolbarStyle::ButtonHeight)))
            {
                editorState.GizmoSnapEnabled = !editorState.GizmoSnapEnabled;
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
                    ImVec2(m_Fonts.HasIcons ? ToolbarStyle::ButtonSize : ToolbarStyle::WideButtonWidth, ToolbarStyle::ButtonHeight)))
            {
                editorState.RequestFocusSelected = true;
            }
        }
        ImGui::End();

        ImGui::PopStyleVar(3);
    }

    void ImGuiLayer::BuildViewport(Editor::EditorState& editorState,
                                   ImTextureID sceneViewportTexture,
                                   const std::function<void()>& viewportOverlayCallback)
    {
        ImGui::Begin("Viewport");
        {
            editorState.ViewportFocused = ImGui::IsWindowFocused();
            editorState.ViewportHovered = ImGui::IsWindowHovered();

            const ImVec2 availableSize = ImGui::GetContentRegionAvail();

            editorState.ViewportWidth = availableSize.x > 1.0f ? static_cast<std::uint32_t>(availableSize.x) : 0;
            editorState.ViewportHeight = availableSize.y > 1.0f ? static_cast<std::uint32_t>(availableSize.y) : 0;

            if (sceneViewportTexture && editorState.ViewportWidth > 0 && editorState.ViewportHeight > 0)
            {
                const ImVec2 imageMin = ImGui::GetCursorScreenPos();
                const ImVec2 imageSize(
                    static_cast<float>(editorState.ViewportWidth),
                    static_cast<float>(editorState.ViewportHeight)
                );

                ImGui::Image(sceneViewportTexture, imageSize);

                editorState.ViewportImageHovered = ImGui::IsItemHovered();
                editorState.ViewportScreenX = imageMin.x;
                editorState.ViewportScreenY = imageMin.y;

                BuildViewportToolbar(editorState, imageMin);

                if (viewportOverlayCallback)
                {
                    viewportOverlayCallback();
                }
            }
            else
            {
                editorState.ViewportImageHovered = false;
                ImGui::TextUnformatted("Scene viewport unavailable.");
            }
        }
        ImGui::End();
    }
}


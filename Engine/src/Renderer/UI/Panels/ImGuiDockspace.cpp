#include "Renderer/UI/ImGuiLayer.h"

#include <algorithm>
#include <filesystem>

#include <imgui.h>
#include <imgui_internal.h>

#include "Renderer/UI/ImGuiTheme.h"

namespace Quiet::Renderer::UI
{
    void ImGuiLayer::BuildDockspace()
    {
        const float toolbarHeight = GetToolbarHeight();
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        if (!viewport) return;

        const ImVec2 dockspacePos = ImVec2(
            viewport->WorkPos.x,
            viewport->WorkPos.y + toolbarHeight
        );

        const ImVec2 dockspaceSize = ImVec2(
            viewport->WorkSize.x,
            viewport->WorkSize.y - toolbarHeight
        );

        if (dockspaceSize.x <= 1.0f || dockspaceSize.y <= 1.0f)
            return;

        ImGuiWindowFlags windowFlags =
            ImGuiWindowFlags_NoDocking |
            ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoBringToFrontOnFocus |
            ImGuiWindowFlags_NoNavFocus |
            ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoScrollWithMouse;

        ImGui::SetNextWindowPos(dockspacePos);
        ImGui::SetNextWindowSize(dockspaceSize);
        ImGui::SetNextWindowViewport(viewport->ID);

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

        ImGui::PushStyleColor(ImGuiCol_WindowBg, EditorColor::Background0);

        ImGui::Begin("##MainDockspaceHost", nullptr, windowFlags);

        ImGui::PopStyleColor();
        ImGui::PopStyleVar(3);

        const ImGuiID dockspaceID = ImGui::GetID("MainDockspace");
        const ImGuiDockNodeFlags dockspaceFlags = ImGuiDockNodeFlags_None;

        if (!m_DefaultLayoutApplied && ShouldApplyDefaultDockLayout())
        {
            ApplyDefaultDockLayout(dockspaceID, dockspaceSize);
            m_DefaultLayoutApplied = true;
        }

        if (m_RequestResetLayout)
        {
            ApplyDefaultDockLayout(dockspaceID, dockspaceSize);
            m_DefaultLayoutApplied = true;
            m_RequestResetLayout = false;
        }

        ImGui::DockSpace(dockspaceID, ImVec2(0.0f, 0.0f), dockspaceFlags);

        ImGui::End();
    }

    void ImGuiLayer::ApplyDefaultDockLayout(ImGuiID dockspaceID, const ImVec2& dockspaceSize)
    {
        if (dockspaceID == 0 || dockspaceSize.x <= 1.0f || dockspaceSize.y <= 1.0f)
        {
            return;
        }

        ImGui::DockBuilderRemoveNode(dockspaceID);
        ImGui::DockBuilderAddNode(dockspaceID, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(dockspaceID, dockspaceSize);

        ImGuiID dockMain = dockspaceID;
        ImGuiID dockLeft = 0;
        ImGuiID dockRight = 0;
        ImGuiID dockBottom = 0;
        ImGuiID dockBottomRight = 0;

        // The ratios are backed by scaled minimum usable panel sizes.
        // This prevents the Inspector and Hierarchy from becoming cramped on
        // high-DPI displays while still leaving the Viewport as the main area.
        const float leftRatio = std::clamp(
            std::max(0.15f, Scale(270.0f) / dockspaceSize.x),
            0.12f,
            0.20f
        );

        const float rightRatioOfFullWidth = std::clamp(
            std::max(0.24f, Scale(390.0f) / dockspaceSize.x),
            0.21f,
            0.30f
        );

        // DockBuilderSplitNode applies the right split to the area remaining
        // after the left split, so compensate to preserve the intended width.
        const float rightRatioOfRemaining = std::clamp(
            rightRatioOfFullWidth / (1.0f - leftRatio),
            0.20f,
            0.40f
        );

        const float bottomRatio = std::clamp(
            std::max(0.31f, Scale(285.0f) / dockspaceSize.y),
            0.27f,
            0.42f
        );

        const float centerWidth =
            dockspaceSize.x * (1.0f - leftRatio - rightRatioOfFullWidth);

        const float debugRatio = std::clamp(
            std::max(0.30f, Scale(325.0f) / std::max(centerWidth, Scale(1.0f))),
            0.28f,
            0.45f
        );

        ImGui::DockBuilderSplitNode(
            dockMain,
            ImGuiDir_Left,
            leftRatio,
            &dockLeft,
            &dockMain
        );

        ImGui::DockBuilderSplitNode(
            dockMain,
            ImGuiDir_Right,
            rightRatioOfRemaining,
            &dockRight,
            &dockMain
        );

        ImGui::DockBuilderSplitNode(
            dockMain,
            ImGuiDir_Down,
            bottomRatio,
            &dockBottom,
            &dockMain
        );

        ImGui::DockBuilderSplitNode(
            dockBottom,
            ImGuiDir_Right,
            debugRatio,
            &dockBottomRight,
            &dockBottom
        );

        ImGui::DockBuilderDockWindow("Viewport", dockMain);
        ImGui::DockBuilderDockWindow("Scene", dockLeft);
        ImGui::DockBuilderDockWindow("Inspector", dockRight);
        ImGui::DockBuilderDockWindow("Scene Settings", dockRight);
        ImGui::DockBuilderDockWindow("Assets", dockBottom);
        ImGui::DockBuilderDockWindow("Console", dockBottom);
        ImGui::DockBuilderDockWindow("Debug", dockBottomRight);

        ImGui::DockBuilderFinish(dockspaceID);
    }

    bool ImGuiLayer::ShouldApplyDefaultDockLayout() const
    {
        const ImGuiIO& io = ImGui::GetIO();
        if (io.IniFilename == nullptr)
        {
            return true;
        }

        return !std::filesystem::exists(io.IniFilename);
    }
}

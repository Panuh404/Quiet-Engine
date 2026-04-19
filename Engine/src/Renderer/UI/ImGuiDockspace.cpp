#include "Renderer/UI/ImGuiLayer.h"

#include <filesystem>

#include <imgui.h>
#include <imgui_internal.h>

namespace Quiet::Renderer::UI
{
    void ImGuiLayer::BuildDockspace()
    {
        const float toolbarHeight = GetToolbarHeight();
        const ImGuiViewport* viewport = ImGui::GetMainViewport();

        const ImVec2 dockspacePos = ImVec2(
            viewport->WorkPos.x,
            viewport->WorkPos.y + toolbarHeight
        );

        const ImVec2 dockspaceSize = ImVec2(
            viewport->WorkSize.x,
            viewport->WorkSize.y - toolbarHeight
        );

        ImGuiWindowFlags windowFlags =
            ImGuiWindowFlags_NoDocking |
            ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoBringToFrontOnFocus |
            ImGuiWindowFlags_NoNavFocus |
            ImGuiWindowFlags_NoBackground;

        ImGui::SetNextWindowPos(dockspacePos);
        ImGui::SetNextWindowSize(dockspaceSize);
        ImGui::SetNextWindowViewport(viewport->ID);

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

        ImGui::Begin("##MainDockspaceHost", nullptr, windowFlags);
        ImGui::PopStyleVar(3);

        const ImGuiID dockspaceID = ImGui::GetID("MainDockspace");
        const ImGuiDockNodeFlags dockspaceFlags = ImGuiDockNodeFlags_PassthruCentralNode;

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
        ImGui::DockBuilderRemoveNode(dockspaceID);
        ImGui::DockBuilderAddNode(dockspaceID, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(dockspaceID, dockspaceSize);

        ImGuiID dockMain = dockspaceID;
        ImGuiID dockLeft = 0;
        ImGuiID dockRight = 0;
        ImGuiID dockBottom = 0;
        ImGuiID dockBottomRight = 0;

        ImGui::DockBuilderSplitNode(dockMain, ImGuiDir_Left, 0.12f, &dockLeft, &dockMain);
        ImGui::DockBuilderSplitNode(dockMain, ImGuiDir_Right, 0.20f, &dockRight, &dockMain);
        ImGui::DockBuilderSplitNode(dockMain, ImGuiDir_Down, 0.25f, &dockBottom, &dockMain);
        ImGui::DockBuilderSplitNode(dockBottom, ImGuiDir_Right, 0.35f, &dockBottomRight, &dockBottom);

        ImGui::DockBuilderDockWindow("Viewport", dockMain);
        ImGui::DockBuilderDockWindow("Scene", dockLeft);
        ImGui::DockBuilderDockWindow("Inspector", dockRight);
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
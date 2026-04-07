#pragma once

#include "imgui.h"
#include <filesystem>


namespace Quiet::Renderer::Vulkan
{
    inline void ApplyDarkEditorTheme(ImGuiIO& io, float uiScale = 1.15f)
    {
        ImGui::StyleColorsDark();

        ImGuiStyle& style = ImGui::GetStyle();
        ImVec4* C = style.Colors;

        style.WindowRounding = 8.0f;
        style.ChildRounding = 8.0f;
        style.FrameRounding = 7.0f;
        style.PopupRounding = 8.0f;
        style.ScrollbarRounding = 9.0f;
        style.GrabRounding = 7.0f;
        style.TabRounding = 10.0f;

        style.WindowBorderSize = 1.0f;
        style.ChildBorderSize = 1.0f;
        style.PopupBorderSize = 1.0f;
        style.FrameBorderSize = 1.0f;
        style.TabBorderSize = 0.0f;

        style.WindowPadding = ImVec2(12.0f, 10.0f);
        style.FramePadding = ImVec2(10.0f, 7.0f);
        style.CellPadding = ImVec2(8.0f, 6.0f);
        style.ItemSpacing = ImVec2(10.0f, 8.0f);
        style.ItemInnerSpacing = ImVec2(8.0f, 6.0f);
        style.TouchExtraPadding = ImVec2(0.0f, 0.0f);
        style.IndentSpacing = 20.0f;
        style.ScrollbarSize = 16.0f;
        style.GrabMinSize = 12.0f;

        style.WindowTitleAlign = ImVec2(0.0f, 0.5f);
        style.ButtonTextAlign = ImVec2(0.5f, 0.5f);
        style.SelectableTextAlign = ImVec2(0.0f, 0.5f);

        // Base
        C[ImGuiCol_Text]                 = ImVec4(0.92f, 0.94f, 0.98f, 1.00f);
        C[ImGuiCol_TextDisabled]         = ImVec4(0.52f, 0.56f, 0.62f, 1.00f);
        C[ImGuiCol_WindowBg]             = ImVec4(0.02f, 0.02f, 0.02f, 1.00f);
        C[ImGuiCol_ChildBg]              = ImVec4(0.06f, 0.06f, 0.06f, 1.00f);
        C[ImGuiCol_PopupBg]              = ImVec4(0.10f, 0.11f, 0.13f, 0.98f);
        C[ImGuiCol_Border]               = ImVec4(0.18f, 0.21f, 0.26f, 1.00f);
        C[ImGuiCol_BorderShadow]         = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);

        // Headers / sections
        C[ImGuiCol_Header]               = ImVec4(0.16f, 0.20f, 0.28f, 1.00f);
        C[ImGuiCol_HeaderHovered]        = ImVec4(0.22f, 0.30f, 0.43f, 1.00f);
        C[ImGuiCol_HeaderActive]         = ImVec4(0.18f, 0.26f, 0.38f, 1.00f);

        // Buttons
        C[ImGuiCol_Button]               = ImVec4(0.14f, 0.17f, 0.22f, 1.00f);
        C[ImGuiCol_ButtonHovered]        = ImVec4(0.20f, 0.28f, 0.40f, 1.00f);
        C[ImGuiCol_ButtonActive]         = ImVec4(0.17f, 0.24f, 0.34f, 1.00f);

        // Inputs
        C[ImGuiCol_FrameBg]              = ImVec4(0.10f, 0.12f, 0.15f, 1.00f);
        C[ImGuiCol_FrameBgHovered]       = ImVec4(0.14f, 0.18f, 0.24f, 1.00f);
        C[ImGuiCol_FrameBgActive]        = ImVec4(0.17f, 0.23f, 0.31f, 1.00f);

        // Tabs
        C[ImGuiCol_Tab]                  = ImVec4(0.10f, 0.12f, 0.15f, 1.00f);
        C[ImGuiCol_TabHovered]           = ImVec4(0.19f, 0.27f, 0.39f, 1.00f);
        C[ImGuiCol_TabActive]            = ImVec4(0.15f, 0.22f, 0.32f, 1.00f);
        C[ImGuiCol_TabUnfocused]         = ImVec4(0.08f, 0.09f, 0.11f, 1.00f);
        C[ImGuiCol_TabUnfocusedActive]   = ImVec4(0.11f, 0.14f, 0.19f, 1.00f);

        // Title bars
        C[ImGuiCol_TitleBg]              = ImVec4(0.06f, 0.07f, 0.09f, 1.00f);
        C[ImGuiCol_TitleBgActive]        = ImVec4(0.10f, 0.14f, 0.20f, 1.00f);
        C[ImGuiCol_TitleBgCollapsed]     = ImVec4(0.05f, 0.06f, 0.07f, 1.00f);

        // Scrollbars
        C[ImGuiCol_ScrollbarBg]          = ImVec4(0.05f, 0.06f, 0.08f, 1.00f);
        C[ImGuiCol_ScrollbarGrab]        = ImVec4(0.18f, 0.22f, 0.28f, 1.00f);
        C[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.24f, 0.31f, 0.42f, 1.00f);
        C[ImGuiCol_ScrollbarGrabActive]  = ImVec4(0.19f, 0.26f, 0.36f, 1.00f);

        // Accent / interaction
        C[ImGuiCol_CheckMark]            = ImVec4(0.38f, 0.68f, 1.00f, 1.00f);
        C[ImGuiCol_SliderGrab]           = ImVec4(0.34f, 0.63f, 0.98f, 1.00f);
        C[ImGuiCol_SliderGrabActive]     = ImVec4(0.52f, 0.76f, 1.00f, 1.00f);
        C[ImGuiCol_ResizeGrip]           = ImVec4(0.24f, 0.31f, 0.41f, 0.60f);
        C[ImGuiCol_ResizeGripHovered]    = ImVec4(0.36f, 0.56f, 0.88f, 0.80f);
        C[ImGuiCol_ResizeGripActive]     = ImVec4(0.40f, 0.68f, 1.00f, 1.00f);
        C[ImGuiCol_Separator]            = ImVec4(0.18f, 0.22f, 0.28f, 1.00f);
        C[ImGuiCol_SeparatorHovered]     = ImVec4(0.30f, 0.46f, 0.72f, 1.00f);
        C[ImGuiCol_SeparatorActive]      = ImVec4(0.38f, 0.62f, 0.98f, 1.00f);

        // Docking
        C[ImGuiCol_DockingPreview]       = ImVec4(0.25f, 0.52f, 0.90f, 0.70f);
        C[ImGuiCol_DockingEmptyBg]       = ImVec4(0.06f, 0.07f, 0.09f, 1.00f);

        // Selection / plot / misc
        C[ImGuiCol_TextSelectedBg]       = ImVec4(0.22f, 0.40f, 0.68f, 0.45f);
        C[ImGuiCol_NavCursor]            = ImVec4(0.38f, 0.68f, 1.00f, 1.00f);
        C[ImGuiCol_MenuBarBg]            = ImVec4(0.08f, 0.09f, 0.11f, 1.00f);

        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            style.WindowRounding = 8.0f;
            style.Colors[ImGuiCol_WindowBg].w = 1.0f;
        }

        style.ScaleAllSizes(uiScale);
    }
}
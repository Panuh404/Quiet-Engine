#pragma once

#include "imgui.h"

namespace Quiet::Renderer::UI
{
    namespace EditorColor
    {
        inline constexpr ImVec4 Text        = ImVec4(0.86f, 0.88f, 0.92f, 1.00f);
        inline constexpr ImVec4 TextMuted   = ImVec4(0.50f, 0.54f, 0.61f, 1.00f);
        inline constexpr ImVec4 TextDim     = ImVec4(0.30f, 0.34f, 0.40f, 1.00f);

        inline constexpr ImVec4 Background0 = ImVec4(0.010f, 0.012f, 0.017f, 1.00f); // app shell / topbar
        inline constexpr ImVec4 Background1 = ImVec4(0.018f, 0.021f, 0.029f, 1.00f); // main windows
        inline constexpr ImVec4 Background2 = ImVec4(0.028f, 0.032f, 0.044f, 1.00f); // cards / child panels
        inline constexpr ImVec4 Background3 = ImVec4(0.045f, 0.052f, 0.070f, 1.00f); // inputs / buttons
        inline constexpr ImVec4 Background4 = ImVec4(0.070f, 0.082f, 0.110f, 1.00f); // hover / raised surface

        inline constexpr ImVec4 Border      = ImVec4(0.095f, 0.110f, 0.145f, 1.00f);
        inline constexpr ImVec4 BorderSoft  = ImVec4(0.060f, 0.070f, 0.095f, 1.00f);

        inline constexpr ImVec4 Accent      = ImVec4(0.180f, 0.480f, 0.950f, 1.00f);
        inline constexpr ImVec4 AccentHover = ImVec4(0.300f, 0.600f, 1.000f, 1.00f);
        inline constexpr ImVec4 AccentSoft  = ImVec4(0.080f, 0.170f, 0.310f, 1.00f);
        inline constexpr ImVec4 AccentDim   = ImVec4(0.045f, 0.100f, 0.190f, 1.00f);

        inline constexpr ImVec4 Success     = ImVec4(0.250f, 0.780f, 0.460f, 1.00f);
        inline constexpr ImVec4 Warning     = ImVec4(0.960f, 0.670f, 0.210f, 1.00f);
        inline constexpr ImVec4 Error       = ImVec4(0.980f, 0.300f, 0.330f, 1.00f);
        inline constexpr ImVec4 Critical    = ImVec4(1.000f, 0.220f, 0.720f, 1.00f);
    }

    namespace EditorStyle
    {
        inline constexpr float WindowRounding = 9.0f;
        inline constexpr float ChildRounding  = 10.0f;
        inline constexpr float FrameRounding  = 7.0f;
        inline constexpr float PopupRounding  = 10.0f;
        inline constexpr float TabRounding    = 8.0f;
    }

    inline void PushToolbarStyle()
    {
        ImGui::PushStyleColor(ImGuiCol_WindowBg, EditorColor::Background0);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, EditorColor::Background0);
        ImGui::PushStyleColor(ImGuiCol_Border, EditorColor::BorderSoft);
        ImGui::PushStyleColor(ImGuiCol_Button, EditorColor::Background2);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, EditorColor::Background4);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, EditorColor::AccentSoft);
    }

    inline void PopToolbarStyle()
    {
        ImGui::PopStyleColor(6);
    }

    inline void DrawMutedText(const char* text)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, EditorColor::TextMuted);
        ImGui::TextUnformatted(text);
        ImGui::PopStyleColor();
    }

    inline void DrawSectionTitle(const char* title)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, EditorColor::Text);
        ImGui::TextUnformatted(title);
        ImGui::PopStyleColor();

        ImGui::PushStyleColor(ImGuiCol_Separator, EditorColor::BorderSoft);
        ImGui::Separator();
        ImGui::PopStyleColor();
    }

    inline void SetPointerCursorOnHover(bool enabled = true)
    {
        if (!enabled) return;
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByPopup))
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    }

    inline bool EditorButton(const char* label, const ImVec2& size = ImVec2(0.0f, 0.0f), bool enabled = true)
    {
        if (!enabled) ImGui::BeginDisabled();

        const bool pressed = ImGui::Button(label, size);
        SetPointerCursorOnHover(enabled);

        if (!enabled) ImGui::EndDisabled();
        return enabled && pressed;
    }

    inline bool EditorBeginMenu(const char* label, bool enabled = true)
    {
        const bool opened = ImGui::BeginMenu(label, enabled);
        SetPointerCursorOnHover(enabled);
        return opened;
    }

    inline bool EditorMenuItem(const char* label, const char* shortcut = nullptr, bool selected = false, bool enabled = true)
    {
        const bool pressed = ImGui::MenuItem(label, shortcut, selected, enabled);
        SetPointerCursorOnHover(enabled);
        return enabled && pressed;
    }

    inline bool EditorCheckbox(const char* label, bool* value, bool enabled = true)
    {
        if (!enabled) ImGui::BeginDisabled();

        const bool changed = ImGui::Checkbox(label, value);
        SetPointerCursorOnHover(enabled);

        if (!enabled) ImGui::EndDisabled();
        return enabled && changed;
    }

    inline bool EditorSelectable(const char* label, bool selected = false, ImGuiSelectableFlags flags = 0,
        const ImVec2& size = ImVec2(0.0f, 0.0f),  bool enabled = true)
    {
        if (!enabled) ImGui::BeginDisabled();

        const bool pressed = ImGui::Selectable(label, selected, flags, size);
        SetPointerCursorOnHover(enabled);

        if (!enabled) ImGui::EndDisabled();
        return enabled && pressed;
    }

    inline bool EditorBeginCombo(const char* label, const char* previewValue, ImGuiComboFlags flags = 0, bool enabled = true)
    {
        if (!enabled) ImGui::BeginDisabled();

        const bool opened = ImGui::BeginCombo(label, previewValue, flags);
        SetPointerCursorOnHover(enabled);

        if (!enabled) ImGui::EndDisabled();
        return enabled && opened;
    }

    inline void ApplyTheme(ImGuiIO& io, float uiScale = 1.15f)
    {
        ImGui::StyleColorsDark();

        ImGuiStyle& style = ImGui::GetStyle();
        ImVec4* C = style.Colors;

        style.WindowRounding = EditorStyle::WindowRounding;
        style.ChildRounding = EditorStyle::ChildRounding;
        style.FrameRounding = EditorStyle::FrameRounding;
        style.PopupRounding = EditorStyle::PopupRounding;
        style.ScrollbarRounding = 10.0f;
        style.GrabRounding = 7.0f;
        style.TabRounding = EditorStyle::TabRounding;

        style.WindowBorderSize = 1.0f;
        style.ChildBorderSize = 1.0f;
        style.PopupBorderSize = 1.0f;
        style.FrameBorderSize = 1.0f;
        style.TabBorderSize = 0.0f;

        style.WindowPadding = ImVec2(12.0f, 10.0f);
        style.FramePadding = ImVec2(10.0f, 7.0f);
        style.CellPadding = ImVec2(9.0f, 7.0f);
        style.ItemSpacing = ImVec2(9.0f, 8.0f);
        style.ItemInnerSpacing = ImVec2(8.0f, 6.0f);
        style.TouchExtraPadding = ImVec2(0.0f, 0.0f);
        style.IndentSpacing = 18.0f;
        style.ScrollbarSize = 14.0f;
        style.GrabMinSize = 12.0f;

        style.WindowTitleAlign = ImVec2(0.0f, 0.5f);
        style.ButtonTextAlign = ImVec2(0.5f, 0.5f);
        style.SelectableTextAlign = ImVec2(0.0f, 0.5f);

        C[ImGuiCol_Text]                 = EditorColor::Text;
        C[ImGuiCol_TextDisabled]         = EditorColor::TextDim;
        C[ImGuiCol_WindowBg]             = EditorColor::Background1;
        C[ImGuiCol_ChildBg]              = EditorColor::Background2;
        C[ImGuiCol_PopupBg]              = ImVec4(0.020f, 0.024f, 0.034f, 0.98f);
        C[ImGuiCol_Border]               = EditorColor::Border;
        C[ImGuiCol_BorderShadow]         = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);

        C[ImGuiCol_Header]               = EditorColor::Background3;
        C[ImGuiCol_HeaderHovered]        = EditorColor::AccentDim;
        C[ImGuiCol_HeaderActive]         = EditorColor::AccentSoft;

        C[ImGuiCol_Button]               = EditorColor::Background3;
        C[ImGuiCol_ButtonHovered]        = EditorColor::Background4;
        C[ImGuiCol_ButtonActive]         = EditorColor::AccentSoft;

        C[ImGuiCol_FrameBg]              = EditorColor::Background3;
        C[ImGuiCol_FrameBgHovered]       = EditorColor::Background4;
        C[ImGuiCol_FrameBgActive]        = EditorColor::AccentDim;

        C[ImGuiCol_Tab]                  = ImVec4(0.025f, 0.030f, 0.042f, 1.00f);
        C[ImGuiCol_TabHovered]           = EditorColor::AccentSoft;
        C[ImGuiCol_TabActive]            = EditorColor::Background3;
        C[ImGuiCol_TabUnfocused]         = EditorColor::Background0;
        C[ImGuiCol_TabUnfocusedActive]   = EditorColor::Background2;

        C[ImGuiCol_TitleBg]              = EditorColor::Background0;
        C[ImGuiCol_TitleBgActive]        = EditorColor::Background2;
        C[ImGuiCol_TitleBgCollapsed]     = EditorColor::Background0;

        C[ImGuiCol_ScrollbarBg]          = EditorColor::Background0;
        C[ImGuiCol_ScrollbarGrab]        = ImVec4(0.090f, 0.105f, 0.135f, 1.00f);
        C[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.140f, 0.170f, 0.220f, 1.00f);
        C[ImGuiCol_ScrollbarGrabActive]  = EditorColor::AccentSoft;

        C[ImGuiCol_CheckMark]            = EditorColor::AccentHover;
        C[ImGuiCol_SliderGrab]           = EditorColor::Accent;
        C[ImGuiCol_SliderGrabActive]     = EditorColor::AccentHover;
        C[ImGuiCol_ResizeGrip]           = ImVec4(0.090f, 0.110f, 0.145f, 0.55f);
        C[ImGuiCol_ResizeGripHovered]    = ImVec4(0.180f, 0.360f, 0.650f, 0.80f);
        C[ImGuiCol_ResizeGripActive]     = EditorColor::AccentHover;
        C[ImGuiCol_Separator]            = EditorColor::BorderSoft;
        C[ImGuiCol_SeparatorHovered]     = EditorColor::AccentSoft;
        C[ImGuiCol_SeparatorActive]      = EditorColor::Accent;

        C[ImGuiCol_DockingPreview]       = ImVec4(0.23f, 0.52f, 0.94f, 0.58f);
        C[ImGuiCol_DockingEmptyBg]       = EditorColor::Background0;

        C[ImGuiCol_TextSelectedBg]       = ImVec4(0.23f, 0.52f, 0.94f, 0.35f);
        C[ImGuiCol_NavCursor]            = EditorColor::AccentHover;
        C[ImGuiCol_MenuBarBg]            = EditorColor::Background0;

        C[ImGuiCol_TableHeaderBg]        = EditorColor::Background2;
        C[ImGuiCol_TableBorderStrong]    = EditorColor::Border;
        C[ImGuiCol_TableBorderLight]     = EditorColor::BorderSoft;
        C[ImGuiCol_TableRowBg]           = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
        C[ImGuiCol_TableRowBgAlt]        = ImVec4(0.030f, 0.036f, 0.048f, 0.60f);

        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            style.WindowRounding = EditorStyle::WindowRounding;
            style.Colors[ImGuiCol_WindowBg].w = 1.0f;
        }

        style.ScaleAllSizes(uiScale);
    }
}
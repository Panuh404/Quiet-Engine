#include "ImGuiToolbarCommon.h"

namespace Quiet::Renderer::UI
{
    std::string MakeToolbarLabel(const EditorFonts& fonts,
                                 const char* id,
                                 const char* iconUtf8,
                                 const char* fallbackText)
    {
        std::string label = fonts.HasIcons ? iconUtf8 : fallbackText;
        label += "##";
        label += id;
        return label;
    }

    bool DrawToolbarButton(const EditorFonts& fonts,
                           const char* id,
                           const char* iconUtf8,
                           const char* fallbackText,
                           const char* tooltip,
                           const bool active,
                           const bool enabled,
                           const ImVec2& size)
    {
        if (!enabled) ImGui::BeginDisabled();

        ImFont* buttonFont = fonts.HasIcons ? fonts.Icons : fonts.UI;
        const bool pushFont = (buttonFont != nullptr);
        if (pushFont) ImGui::PushFont(buttonFont);

        const bool iconButton = (fonts.HasIcons && buttonFont == fonts.Icons);

        ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.5f, 0.5f));
        if (iconButton)
        {
            const float padY = ImGui::GetStyle().FramePadding.y;
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, padY));
        }

        if (active)
        {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.22f, 0.34f, 0.56f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.27f, 0.40f, 0.64f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.18f, 0.29f, 0.50f, 1.0f));
        }

        const std::string label = MakeToolbarLabel(fonts, id, iconUtf8, fallbackText);
        const bool pressed = ImGui::Button(label.c_str(), size);

        if (active) ImGui::PopStyleColor(3);
        if (iconButton) ImGui::PopStyleVar();
        ImGui::PopStyleVar();
        if (pushFont) ImGui::PopFont();
        if (!enabled) ImGui::EndDisabled();

        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
            ImGui::SetTooltip("%s", tooltip);

        return pressed && enabled;
    }
}
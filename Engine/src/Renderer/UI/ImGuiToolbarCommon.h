#pragma once

#include <string>
#include <imgui.h>

#include "ImGuiFonts.h"
#include "ImGuiTheme.h"

namespace Quiet::Renderer::UI
{
    namespace Icons
    {
        inline constexpr const char* New       = "\xef\x85\x9b"; // U+F15B file
        inline constexpr const char* Save      = "\xef\x83\x87"; // U+F0C7 floppy-disk
        inline constexpr const char* Load      = "\xef\x81\xbc"; // U+F07C folder-open
        inline constexpr const char* Play      = "\xef\x81\x8b"; // U+F04B play
        inline constexpr const char* Stop      = "\xef\x81\x8d"; // U+F04D stop
        inline constexpr const char* Simulate  = "\xef\x80\xa1"; // U+F021 rotate/sync

        inline constexpr const char* Gear =     "\xef\x80\x93"; // U+F013 cog

        inline constexpr const char* Translate = "\xef\x81\x87"; // U+F047 arrows
        inline constexpr const char* Rotate    = "\xef\x80\xa1"; // U+F021 rotate/sync
        inline constexpr const char* Scale     = "\xef\x81\xa5"; // U+F065 expand
        inline constexpr const char* Focus     = "\xef\x81\x9b"; // U+F05B crosshairs
        inline constexpr const char* Snap      = "\xef\x81\xb6"; // U+F076 magnet
        inline constexpr const char* DebugIcons = "\xef\x81\xae"; // U+F06E eye
        inline constexpr const char* World     = "\xef\x82\xac"; // globe
        inline constexpr const char* Local     = "\xef\x86\xb2"; // cube
    }

    namespace ToolbarStyle
    {
        inline float ButtonHeight()
        {
            const float minimumHeight = EditorSizing::Px(36.0f);

            return (ImGui::GetFrameHeight() > minimumHeight)
                ? ImGui::GetFrameHeight()
                : minimumHeight;
        }

        inline float ButtonSize() { return ButtonHeight(); }
        inline float WideButtonWidth() { return ButtonHeight() * 1.65f; }
    }

    [[nodiscard]] std::string MakeToolbarLabel(const EditorFonts& fonts,
                                               const char* id,
                                               const char* iconUtf8,
                                               const char* fallbackText);

    bool DrawToolbarButton(const EditorFonts& fonts,
                           const char* id,
                           const char* iconUtf8,
                           const char* fallbackText,
                           const char* tooltip,
                           bool active = false,
                           bool enabled = true,
                           const ImVec2& size = ImVec2(
                               ToolbarStyle::ButtonSize(),
                               ToolbarStyle::ButtonHeight()));
}
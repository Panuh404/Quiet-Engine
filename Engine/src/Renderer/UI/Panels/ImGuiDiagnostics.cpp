#include "Renderer/UI/ImGuiLayer.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <string>
#include <cstdio>

#include <imgui.h>

#include "Quiet/Core/Log.h"
#include "Renderer/UI/ImGuiTheme.h"

namespace
{
    namespace QUI = Quiet::Renderer::UI;
    namespace SZ = Quiet::Renderer::UI::EditorSizing;

    const char* GetLevelLabel(const spdlog::level::level_enum level)
    {
        switch (level)
        {
            case spdlog::level::trace:    return "TRACE";
            case spdlog::level::debug:    return "DEBUG";
            case spdlog::level::info:     return "INFO";
            case spdlog::level::warn:     return "WARN";
            case spdlog::level::err:      return "ERROR";
            case spdlog::level::critical: return "CRITICAL";
            default:                      return "LOG";
        }
    }

    ImVec4 GetLevelColor(const spdlog::level::level_enum level)
    {
        switch (level)
        {
            case spdlog::level::trace:
            case spdlog::level::debug:
                return QUI::EditorColor::TextDim;

            case spdlog::level::info:
                return QUI::EditorColor::Text;

            case spdlog::level::warn:
                return QUI::EditorColor::Warning;

            case spdlog::level::err:
                return QUI::EditorColor::Error;

            case spdlog::level::critical:
                return QUI::EditorColor::Critical;

            default:
                return QUI::EditorColor::Text;
        }
    }

    struct ConsoleCounts
    {
        std::size_t Total = 0;
        std::size_t Trace = 0;
        std::size_t Info = 0;
        std::size_t Warnings = 0;
        std::size_t Errors = 0;
        std::size_t Critical = 0;
    };

    bool ContainsCaseInsensitive(const std::string& text, const char* filter)
    {
        if (!filter || filter[0] == '\0') return true;

        const std::string filterText(filter);
        return std::search(
            text.begin(),
            text.end(),
            filterText.begin(),
            filterText.end(),
            [](const char a, const char b)
            {
                return std::tolower(static_cast<unsigned char>(a)) ==
                       std::tolower(static_cast<unsigned char>(b));
            }
        ) != text.end();
    }

    template <typename Entries>
    ConsoleCounts BuildConsoleCounts(const Entries& entries)
    {
        ConsoleCounts counts{};
        counts.Total = entries.size();

        for (const auto& entry : entries)
        {
            switch (entry.Level)
            {
                case spdlog::level::trace:
                case spdlog::level::debug:
                    ++counts.Trace;
                    break;

                case spdlog::level::info:
                    ++counts.Info;
                    break;

                case spdlog::level::warn:
                    ++counts.Warnings;
                    break;

                case spdlog::level::err:
                    ++counts.Errors;
                    break;

                case spdlog::level::critical:
                    ++counts.Critical;
                    break;

                default:
                    break;
            }
        }

        return counts;
    }

    void DrawConsoleStat(const char* label, const std::size_t value, const ImVec4& color)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, color);
        ImGui::Text("%s: %zu", label, value);
        ImGui::PopStyleColor();
    }

    void DrawDebugMetric(const char* label, const char* value)
    {
        ImGui::TableNextRow();

        ImGui::TableSetColumnIndex(0);

        ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::TextMuted);
        ImGui::TextUnformatted(label);
        ImGui::PopStyleColor();

        ImGui::TableSetColumnIndex(1);

        ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::Text);
        ImGui::TextUnformatted(value);
        ImGui::PopStyleColor();
    }

    template <typename Entry>
    void DrawConsoleEntry(const Entry& entry, const int rowIndex, ImFont* monoFont)
    {
        const ImVec4 levelColor = GetLevelColor(entry.Level);
        const char* levelLabel = GetLevelLabel(entry.Level);

        const float rowHeight = ImGui::GetTextLineHeight() + SZ::Px(5.0f);
        const float labelWidth = SZ::Px(58.0f);
        const float leftPadding = SZ::Px(6.0f);
        const float textPadding = SZ::Px(6.0f);

        const ImVec2 textSize = ImGui::CalcTextSize(entry.Text.c_str());
        const float visibleWidth = ImGui::GetContentRegionAvail().x;
        const float rowWidth = std::max(
            visibleWidth,
            labelWidth + leftPadding + textPadding + textSize.x + SZ::Px(24.0f)
        );

        const ImVec2 rowMin = ImGui::GetCursorScreenPos();
        const ImVec2 rowMax = ImVec2(rowMin.x + rowWidth, rowMin.y + rowHeight);

        ImGui::PushID(rowIndex);
        ImGui::InvisibleButton("##ConsoleEntry", ImVec2(rowWidth, rowHeight));
        const bool hovered = ImGui::IsItemHovered();
        ImGui::PopID();

        ImDrawList* drawList = ImGui::GetWindowDrawList();

        if (hovered)
        {
            drawList->AddRectFilled(
                rowMin,
                rowMax,
                ImGui::ColorConvertFloat4ToU32(QUI::EditorColor::Background3),
                SZ::Px(5.0f)
            );
        }

        drawList->AddRectFilled(
            ImVec2(rowMin.x, rowMin.y + SZ::Px(3.0f)),
            ImVec2(rowMin.x + SZ::Px(2.0f), rowMax.y - SZ::Px(3.0f)),
            ImGui::ColorConvertFloat4ToU32(levelColor),
            SZ::Px(2.0f)
        );

        const ImVec2 labelPos = ImVec2(
            rowMin.x + leftPadding,
            rowMin.y + SZ::Px(2.0f)
        );

        drawList->AddText(
            labelPos,
            ImGui::ColorConvertFloat4ToU32(levelColor),
            levelLabel
        );

        const ImVec2 textPos = ImVec2(
            rowMin.x + labelWidth + textPadding,
            rowMin.y + SZ::Px(2.0f)
        );

        if (monoFont)
        {
            drawList->AddText(
                monoFont,
                monoFont->FontSize,
                textPos,
                ImGui::ColorConvertFloat4ToU32(
                    entry.Level >= spdlog::level::warn ? levelColor : QUI::EditorColor::Text
                ),
                entry.Text.c_str()
            );
        }
        else
        {
            drawList->AddText(
                textPos,
                ImGui::ColorConvertFloat4ToU32(
                    entry.Level >= spdlog::level::warn ? levelColor : QUI::EditorColor::Text
                ),
                entry.Text.c_str()
            );
        }
    }
}

namespace Quiet::Renderer::UI
{
    void ImGuiLayer::BuildDebugWindow(const ImGuiStats& stats)
    {
        ImGui::Begin("Debug");
        {
            char frameTimeBuffer[64]{};
            std::snprintf(
                frameTimeBuffer,
                sizeof(frameTimeBuffer),
                "%.2f ms",
                stats.FrameTimeMs
            );

            char extentBuffer[64]{};
            std::snprintf(
                extentBuffer,
                sizeof(extentBuffer),
                "%u x %u",
                stats.SwapchainExtent.width,
                stats.SwapchainExtent.height
            );

            char positionBuffer[128]{};
            std::snprintf(
                positionBuffer,
                sizeof(positionBuffer),
                "%.1f, %.1f, %.1f",
                stats.CameraPosition.x,
                stats.CameraPosition.y,
                stats.CameraPosition.z
            );

            ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, SZ::Px(ImVec2(6.0f, 4.0f)));

            if (ImGui::BeginTable("##DebugMetrics", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_BordersInnerH))
            {
                ImGui::TableSetupColumn("Metric", ImGuiTableColumnFlags_WidthFixed, SZ::Px(88.0f)
                );

                ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

                DrawDebugMetric("FPS", std::to_string(static_cast<int>(stats.FPS)).c_str());
                DrawDebugMetric("Frame", frameTimeBuffer);
                DrawDebugMetric("Actors", std::to_string(stats.ActorCount).c_str());
                DrawDebugMetric("Meshes", std::to_string(stats.MeshCount).c_str());
                DrawDebugMetric("Swapchain", extentBuffer);
                DrawDebugMetric("Mouse", stats.MouseCaptured ? "Captured" : "Free");
                DrawDebugMetric("Camera", positionBuffer);

                ImGui::EndTable();
            }

            ImGui::PopStyleVar();
        }
        ImGui::End();
    }

    void ImGuiLayer::BuildConsoleWindow()
    {
        ImGui::Begin("Console");
        {
            auto consoleSink = Quiet::Core::Log::GetConsoleSink();
            if (!consoleSink)
            {
                DrawMutedText("Console sink unavailable.");
                ImGui::End();
                return;
            }

            static std::array<char, 256> filterBuffer{};
            static bool autoScroll = true;

            const auto entries = consoleSink->GetSnapshot();
            const ConsoleCounts counts = BuildConsoleCounts(entries);

            auto BuildClipboardText = [&](const bool onlyVisible) -> std::string
            {
                std::string result;
                result.reserve(entries.size() * 96);

                for (const auto& entry : entries)
                {
                    if (onlyVisible && !ContainsCaseInsensitive(entry.Text, filterBuffer.data()))
                        continue;

                    result += entry.Text;
                    if (result.empty() || result.back() != '\n')
                        result += '\n';
                }
                return result;
            };

            const ImVec2 toolbarPadding = SZ::Px(ImVec2(12.0f, 10.0f));

            ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, SZ::Px(10.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, toolbarPadding);

            ImGui::PushStyleColor(ImGuiCol_ChildBg, EditorColor::Background2);
            ImGui::PushStyleColor(ImGuiCol_Border, EditorColor::BorderSoft);

            const float toolbarHeight =
                toolbarPadding.y * 2.0f +
                ImGui::GetFrameHeight() * 2.0f +
                ImGui::GetStyle().ItemSpacing.y +
                SZ::Px(6.0f);

            ImGui::BeginChild("##ConsoleToolbar", ImVec2(0.0f, toolbarHeight), ImGuiChildFlags_Borders);
            {
                const float availableWidth = ImGui::GetContentRegionAvail().x;
                const bool wideToolbar = availableWidth >= SZ::Px(900.0f);

                ImGui::PushStyleColor(ImGuiCol_Text, EditorColor::Text);
                ImGui::AlignTextToFramePadding();
                ImGui::TextUnformatted("Runtime Console");
                ImGui::PopStyleColor();

                ImGui::SameLine();

                ImGui::PushStyleColor(ImGuiCol_Text, EditorColor::TextMuted);
                ImGui::Text("(%zu)", counts.Total);
                ImGui::PopStyleColor();

                ImGui::SameLine();

                DrawConsoleStat("Trace", counts.Trace, EditorColor::TextDim);
                ImGui::SameLine();
                DrawConsoleStat("Info", counts.Info, EditorColor::Text);
                ImGui::SameLine();
                DrawConsoleStat("Warnings", counts.Warnings, EditorColor::Warning);
                ImGui::SameLine();
                DrawConsoleStat("Errors", counts.Errors, EditorColor::Error);
                ImGui::SameLine();
                DrawConsoleStat("Critical", counts.Critical, EditorColor::Critical);

                if (EditorButton("Clear", SZ::Px(ImVec2(70.0f, 0.0f))))
                {
                    consoleSink->Clear();
                }

                ImGui::SameLine();

                if (EditorButton("Copy All", SZ::Px(ImVec2(84.0f, 0.0f))))
                {
                    const std::string clipboardText = BuildClipboardText(false);
                    ImGui::SetClipboardText(clipboardText.c_str());
                }

                ImGui::SameLine();

                if (EditorButton("Copy Visible", SZ::Px(ImVec2(104.0f, 0.0f))))
                {
                    const std::string clipboardText = BuildClipboardText(true);
                    ImGui::SetClipboardText(clipboardText.c_str());
                }

                ImGui::SameLine();

                EditorCheckbox("Auto-scroll", &autoScroll);

                if (wideToolbar)
                {
                    ImGui::SameLine();

                    const float searchWidth = SZ::Px(300.0f);
                    const float searchX =
                        ImGui::GetCursorPosX() +
                        ImGui::GetContentRegionAvail().x -
                        searchWidth;

                    if (searchX > ImGui::GetCursorPosX() + SZ::Px(16.0f))
                    {
                        ImGui::SetCursorPosX(searchX);
                    }

                    ImGui::SetNextItemWidth(searchWidth);
                    ImGui::InputTextWithHint(
                        "##ConsoleSearch",
                        "Search logs...",
                        filterBuffer.data(),
                        filterBuffer.size()
                    );
                }
                else
                {
                    ImGui::SetNextItemWidth(-1.0f);
                    ImGui::InputTextWithHint(
                        "##ConsoleSearch",
                        "Search logs...",
                        filterBuffer.data(),
                        filterBuffer.size()
                    );
                }
            }
            ImGui::EndChild();

            ImGui::PopStyleColor(2);
            ImGui::PopStyleVar(2);

            ImGui::Spacing();

            ImGuiWindowFlags childFlags = ImGuiWindowFlags_HorizontalScrollbar;

            ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, SZ::Px(10.0f));
            ImGui::PushStyleColor(ImGuiCol_ChildBg, EditorColor::Background1);
            ImGui::PushStyleColor(ImGuiCol_Border, EditorColor::BorderSoft);

            ImGui::BeginChild(
                "##ConsoleRegion",
                ImVec2(0.0f, 0.0f),
                ImGuiChildFlags_Borders,
                childFlags
            );
            {
                const bool wasAtBottom =
                    ImGui::GetScrollY() >= (ImGui::GetScrollMaxY() - 8.0f);

                if (ImGui::BeginPopupContextWindow())
                {
                    if (EditorSelectable("Clear"))
                    {
                        consoleSink->Clear();
                    }

                    if (EditorSelectable("Copy All"))
                    {
                        const std::string clipboardText = BuildClipboardText(false);
                        ImGui::SetClipboardText(clipboardText.c_str());
                    }

                    if (EditorSelectable("Copy Visible"))
                    {
                        const std::string clipboardText = BuildClipboardText(true);
                        ImGui::SetClipboardText(clipboardText.c_str());
                    }

                    ImGui::EndPopup();
                }

                int visibleRowIndex = 0;

                for (const auto& entry : entries)
                {
                    if (!ContainsCaseInsensitive(entry.Text, filterBuffer.data()))
                        continue;

                    DrawConsoleEntry(entry, visibleRowIndex, m_Fonts.Mono);
                    ++visibleRowIndex;
                }

                if (visibleRowIndex == 0)
                {
                    ImGui::Spacing();
                    DrawMutedText("No console entries match the current filter.");
                }

                if (autoScroll && wasAtBottom)
                {
                    ImGui::SetScrollHereY(1.0f);
                }
            }
            ImGui::EndChild();

            ImGui::PopStyleColor(2);
            ImGui::PopStyleVar();
        }
        ImGui::End();
    }
}
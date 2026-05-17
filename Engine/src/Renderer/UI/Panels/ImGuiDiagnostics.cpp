#include "Renderer/UI/ImGuiLayer.h"

#include <imgui.h>

#include "Quiet/Core/Log.h"
#include "Renderer/UI/ImGuiTheme.h"

namespace Quiet::Renderer::UI
{
    void ImGuiLayer::BuildDebugWindow(const ImGuiStats& stats)
    {
        ImGui::Begin("Debug");
        {
            ImGui::Text("FPS: %.1f", stats.FPS);
            ImGui::Text("Frame Time: %.3f ms", stats.FrameTimeMs);
            ImGui::Separator();

            ImGui::Text("Actors: %u", stats.ActorCount);
            ImGui::Text("Mesh Resources: %u", stats.MeshCount);
            ImGui::Text("Swapchain Extent: %u x %u",
                        stats.SwapchainExtent.width,
                        stats.SwapchainExtent.height);
            ImGui::Separator();

            ImGui::Text("Camera:");
            ImGui::Text("Mouse Capture: %s", stats.MouseCaptured ? "True" : "False");
            ImGui::Text("X: %.2f", stats.CameraPosition.x);
            ImGui::Text("Y: %.2f", stats.CameraPosition.y);
            ImGui::Text("Z: %.2f", stats.CameraPosition.z);
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
                ImGui::TextUnformatted("Console sink unavailable.");
                ImGui::End();
                return;
            }

            static ImGuiTextFilter filter;
            static bool autoScroll = true;

            const auto entries = consoleSink->GetSnapshot();

            auto BuildClipboardText = [&](bool onlyVisible) -> std::string
            {
                std::string result;
                result.reserve(entries.size() * 96);

                for (const auto& entry : entries)
                {
                    if (onlyVisible && !filter.PassFilter(entry.Text.c_str()))
                    {
                        continue;
                    }

                    result += entry.Text;
                    if (result.empty() || result.back() != '\n')
                    {
                        result += '\n';
                    }
                }

                return result;
            };

            if (EditorButton("Clear"))
            {
                consoleSink->Clear();
            }

            ImGui::SameLine();
            if (EditorButton("Copy All"))
            {
                const std::string clipboardText = BuildClipboardText(false);
                ImGui::SetClipboardText(clipboardText.c_str());
            }

            ImGui::SameLine();
            if (EditorButton("Copy Visible"))
            {
                const std::string clipboardText = BuildClipboardText(true);
                ImGui::SetClipboardText(clipboardText.c_str());
            }

            ImGui::SameLine();
            EditorCheckbox("Auto-scroll", &autoScroll);

            ImGui::SameLine();
            filter.Draw("Filter", 260.0f);

            ImGui::Separator();

            ImGuiWindowFlags childFlags = ImGuiWindowFlags_HorizontalScrollbar;
            ImGui::BeginChild("##ConsoleRegion", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders, childFlags);
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

                for (const auto& entry : entries)
                {
                    if (!filter.PassFilter(entry.Text.c_str()))
                    {
                        continue;
                    }

                    ImVec4 color{};
                    switch (entry.Level)
                    {
                        case spdlog::level::trace:
                            color = EditorColor::TextDim;
                            break;

                        case spdlog::level::info:
                            color = EditorColor::Text;
                            break;

                        case spdlog::level::warn:
                            color = EditorColor::Warning;
                            break;

                        case spdlog::level::err:
                            color = EditorColor::Error;
                            break;

                        case spdlog::level::critical:
                            color = EditorColor::Critical;
                            break;

                        default:
                            color = EditorColor::Text;
                            break;
                    }

                    ImGui::PushStyleColor(ImGuiCol_Text, color);
                    if (m_Fonts.Mono)
                    {
                        ImGui::PushFont(m_Fonts.Mono);
                    }

                    ImGui::TextUnformatted(entry.Text.c_str());

                    if (m_Fonts.Mono)
                    {
                        ImGui::PopFont();
                    }
                    ImGui::PopStyleColor();
                }

                if (autoScroll && wasAtBottom)
                {
                    ImGui::SetScrollHereY(1.0f);
                }
            }
            ImGui::EndChild();
        }
        ImGui::End();
    }
}

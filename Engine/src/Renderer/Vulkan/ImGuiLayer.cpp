#include "ImGuiLayer.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_vulkan.h>
#include <ImGuizmo.h>

#include <GLFW/glfw3.h>
#include <stdexcept>

#include "ImGuiTheme.h"
#include "MeshLibrary.h"
#include "Quiet/Editor/EditorState.h"
#include "Quiet/Core/Log.h"
#include "Quiet/Editor/EditorActionID.h"
#include "Quiet/Editor/EditorActions.h"
#include "Quiet/Editor/EditorCommands.h"
#include "Quiet/Editor/EditorController.h"
#include "Scene/Scene.h"

namespace
{
    constexpr const char* kIconNew       = "\xef\x85\x9b"; // U+F15B file
    constexpr const char* kIconSave      = "\xef\x83\x87"; // U+F0C7 floppy-disk
    constexpr const char* kIconLoad      = "\xef\x81\xbc"; // U+F07C folder-open
    constexpr const char* kIconPlay      = "\xef\x81\x8b"; // U+F04B play
    constexpr const char* kIconStop      = "\xef\x81\x8d"; // U+F04D stop
    constexpr const char* kIconSimulate  = "\xef\x80\xa1"; // U+F021 rotate/sync
    constexpr const char* kIconTranslate = "\xef\x81\x87"; // U+F047 arrows
    constexpr const char* kIconRotate    = "\xef\x80\xa1"; // U+F021 rotate/sync
    constexpr const char* kIconScale     = "\xef\x81\xa5"; // U+F065 expand
    constexpr const char* kIconFocus     = "\xef\x81\x9b"; // U+F05B crosshairs
    constexpr const char* kIconSnap      = "\xef\x81\xb6"; // U+F076 magnet

    constexpr float kToolbarButtonSize = 46.0f;
    constexpr float kToolbarWideButtonWidth = 78.0f;
    constexpr float kToolbarModeButtonWidth = 82.0f;
    constexpr float kToolbarButtonHeight = 48.0f;

    std::string MakeToolbarLabel(const Quiet::Renderer::Vulkan::EditorFonts& fonts,
                             const char* id,
                             const char* iconUtf8,
                             const char* fallbackText)
    {
        std::string label = fonts.HasIcons ? iconUtf8 : fallbackText;
        label += "##";
        label += id;
        return label;
    }

    bool DrawToolbarButton(const Quiet::Renderer::Vulkan::EditorFonts& fonts,
                       const char* id,
                       const char* iconUtf8,
                       const char* fallbackText,
                       const char* tooltip,
                       bool active = false,
                       bool enabled = true,
                       const ImVec2& size = ImVec2(kToolbarButtonSize, kToolbarButtonHeight))
    {
        if (!enabled)
        {
            ImGui::BeginDisabled();
        }

        ImFont* buttonFont = fonts.HasIcons ? fonts.Icons : fonts.UI;
        const bool pushFont = (buttonFont != nullptr);
        if (pushFont)
        {
            ImGui::PushFont(buttonFont);
        }

        if (active)
        {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.22f, 0.34f, 0.56f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.27f, 0.40f, 0.64f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.18f, 0.29f, 0.50f, 1.0f));
        }

        const std::string label = MakeToolbarLabel(fonts, id, iconUtf8, fallbackText);
        const bool pressed = ImGui::Button(label.c_str(), size);

        if (active)
        {
            ImGui::PopStyleColor(3);
        }

        if (pushFont)
        {
            ImGui::PopFont();
        }

        if (!enabled)
        {
            ImGui::EndDisabled();
        }

        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
        {
            ImGui::SetTooltip("%s", tooltip);
        }

        return pressed && enabled;
    }

    const char* GetSceneModeLabel(Quiet::Editor::SceneMode mode)
    {
        switch (mode)
        {
            case Quiet::Editor::SceneMode::Edit:     return "[EDIT]";
            case Quiet::Editor::SceneMode::Simulate: return "[SIMULATE]";
            case Quiet::Editor::SceneMode::Play:     return "[PLAY]";
            default:                                 return "[UNKNOWN]";
        }
    }

    bool DrawMenuAction(Quiet::Editor::EditorActions& actions,
                        Quiet::Editor::EditorActionID actionID)
    {
        const char* shortcut = actions.GetShortcut(actionID);
        if (shortcut[0] == '\0') shortcut = nullptr;

        const bool enabled = actions.CanExecute(actionID);
        const bool pressed = ImGui::MenuItem(
            actions.GetLabel(actionID),
            shortcut,
            false,
            enabled
        );

        if (pressed) actions.Execute(actionID);
        return pressed;
    }

    bool DrawToolbarAction(Quiet::Editor::EditorActions& actions,
                           Quiet::Editor::EditorActionID actionID,
                           const char* buttonLabel = nullptr)
    {
        const bool enabled = actions.CanExecute(actionID);
        if (!enabled) ImGui::BeginDisabled();

        const bool pressed = ImGui::Button(
            buttonLabel ? buttonLabel : actions.GetLabel(actionID)
        );

        if (!enabled) ImGui::EndDisabled();
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
        {
            const char* shortcut = actions.GetShortcut(actionID);
            if (shortcut[0] != '\0') ImGui::SetTooltip("%s", shortcut);
        }

        if (pressed) actions.Execute(actionID);
        return pressed;
    }
}

namespace Quiet::Renderer::Vulkan
{
    ImGuiLayer::ImGuiLayer(GLFWwindow* window,
                           VkInstance instance,
                           VkPhysicalDevice physicalDevice,
                           VkDevice device,
                           std::uint32_t queueFamily,
                           VkQueue queue,
                           std::uint32_t minImageCount,
                           std::uint32_t imageCount,
                           VkFormat colorFormat,
                           VkFormat depthFormat)
        : m_Device(device)
    {
        CreateDescriptorPool();
        // TODO: Set default layout
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

        m_Fonts = Vulkan::SetupEditorFonts(
            io,
            28.0f, // UI
            28.0f, // Bold
            24.0f  // Mono
        );

        ApplyDarkEditorTheme(io);
        ImGui_ImplGlfw_InitForVulkan(window, true);

        ImGui_ImplVulkan_InitInfo initInfo{};
        initInfo.ApiVersion = VK_API_VERSION_1_3;
        initInfo.Instance = instance;
        initInfo.PhysicalDevice = physicalDevice;
        initInfo.Device = device;
        initInfo.QueueFamily = queueFamily;
        initInfo.Queue = queue;
        initInfo.DescriptorPool = m_DescriptorPool;
        initInfo.MinImageCount = minImageCount;
        initInfo.ImageCount = imageCount;
        initInfo.UseDynamicRendering = true;
        initInfo.CheckVkResultFn = nullptr;

        VkPipelineRenderingCreateInfoKHR pipelineRenderingInfo{};
        pipelineRenderingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
        pipelineRenderingInfo.colorAttachmentCount = 1;
        pipelineRenderingInfo.pColorAttachmentFormats = &colorFormat;
        pipelineRenderingInfo.depthAttachmentFormat = depthFormat;
        pipelineRenderingInfo.stencilAttachmentFormat = VK_FORMAT_UNDEFINED;

        initInfo.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
        initInfo.PipelineInfoMain.PipelineRenderingCreateInfo = pipelineRenderingInfo;

        if (!ImGui_ImplVulkan_Init(&initInfo))
        {
            throw std::runtime_error("Failed to initialize ImGui Vulkan backend.");
        }

        QUIET_CORE_INFO("Initialized Dear ImGui GLFW + Vulkan debug overlay.");
    }

    ImGuiLayer::~ImGuiLayer()
    {
        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        DestroyDescriptorPool();
    }

    void ImGuiLayer::BeginFrame()
    {
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        ImGuizmo::BeginFrame();
    }

    void ImGuiLayer::BuildEditorUI(const ImGuiStats& stats,
                                   Input::Input& input,
                                   Scene::Scene& scene,
                                   Editor::EditorController& editorController,
                                   Editor::EditorState& editorState,
                                   const MeshLibrary& meshLibrary,
                                   ImTextureID sceneViewportTexture,
                                   const std::function<void()>& viewportOverlayCallback)
    {
        Editor::EditorCommands commands(editorController);
        Editor::EditorActions actions(input, scene, commands, editorState);

        BuildMainMenuBar(actions);
        BuildTopbar(actions, editorState);
        BuildDockspace();

        BuildViewport(editorState, sceneViewportTexture, viewportOverlayCallback);

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

        BuildInspector(scene, commands, editorState, meshLibrary);
        BuildHierarchy(scene, actions, editorState);
        BuildConsoleWindow();

        actions.HandleShortcuts(stats.MouseCaptured);
    }

    void ImGuiLayer::EndFrame()
    {
        ImGui::Render();
    }

    void ImGuiLayer::Render(VkCommandBuffer commandBuffer) const
    {
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer);
    }

    bool ImGuiLayer::WantsMouseCapture() const
    {
        return ImGui::GetIO().WantCaptureMouse;
    }

    bool ImGuiLayer::WantsKeyboardCapture() const
    {
        return ImGui::GetIO().WantCaptureKeyboard;
    }

    float ImGuiLayer::GetToolbarHeight() const
    {
        const ImGuiStyle& style = ImGui::GetStyle();

        const float textHeight = ImGui::GetFontSize();
        const float frameHeight = textHeight + style.FramePadding.y * 2.0f;
        const float contentHeight =
            (frameHeight > kToolbarButtonHeight) ? frameHeight : kToolbarButtonHeight;

        return contentHeight + style.WindowPadding.y * 2.0f + 6.0f;
    }

    void ImGuiLayer::BuildMainMenuBar(Editor::EditorActions& editorActions)
    {
        if (!ImGui::BeginMainMenuBar()) return;
        if (ImGui::BeginMenu("File"))
        {
            DrawMenuAction(editorActions, Editor::EditorActionID::NewScene);
            DrawMenuAction(editorActions, Editor::EditorActionID::SaveScene);
            DrawMenuAction(editorActions, Editor::EditorActionID::LoadScene);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Edit"))
        {
            DrawMenuAction(editorActions, Editor::EditorActionID::Undo);
            DrawMenuAction(editorActions, Editor::EditorActionID::Redo);
            ImGui::Separator();
            DrawMenuAction(editorActions, Editor::EditorActionID::DuplicateSelected);
            DrawMenuAction(editorActions, Editor::EditorActionID::DeleteSelected);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Scene"))
        {
            DrawMenuAction(editorActions, Editor::EditorActionID::CreateEmptyActor);
            DrawMenuAction(editorActions, Editor::EditorActionID::FocusSelected);
            ImGui::Separator();
            DrawMenuAction(editorActions, Editor::EditorActionID::ToggleSimulate);
            DrawMenuAction(editorActions, Editor::EditorActionID::TogglePlay);
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }

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
        ImGuiDockNodeFlags dockspaceFlags = ImGuiDockNodeFlags_PassthruCentralNode;

        ImGui::DockSpace(dockspaceID, ImVec2(0.0f, 0.0f), dockspaceFlags);

        ImGui::End();
    }

    void ImGuiLayer::BuildViewportToolbar(Editor::EditorState& editorState,
                                          const ImVec2& imageMin)
    {
        if (editorState.CurrentSceneMode != Editor::SceneMode::Edit)
        {
            return;
        }

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
                    kIconTranslate,
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
                    kIconRotate,
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
                    kIconScale,
                    "S",
                    "Scale (S)",
                    editorState.CurrentGizmoOperation == Editor::GizmoOperation::Scale))
            {
                editorState.CurrentGizmoOperation = Editor::GizmoOperation::Scale;
            }

            ImGui::SameLine();
            ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
            ImGui::SameLine();

            if (editorState.CurrentGizmoOperation != Editor::GizmoOperation::Scale)
            {
                const char* spaceLabel =
                    editorState.CurrentGizmoMode == Editor::GizmoMode::Local
                        ? "Local##ViewportSpace"
                        : "World##ViewportSpace";

                if (ImGui::Button(spaceLabel, ImVec2(kToolbarModeButtonWidth, kToolbarButtonHeight)))
                {
                    editorState.CurrentGizmoMode =
                        (editorState.CurrentGizmoMode == Editor::GizmoMode::Local)
                            ? Editor::GizmoMode::World
                            : Editor::GizmoMode::Local;
                }

                if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
                {
                    ImGui::SetTooltip("Toggle Local / World (L)");
                }

                ImGui::SameLine();
            }

            if (DrawToolbarButton(
                    m_Fonts,
                    "ViewportSnap",
                    kIconSnap,
                    "Snap",
                    "Toggle Snap (G)",
                    editorState.GizmoSnapEnabled,
                    true,
                    ImVec2(m_Fonts.HasIcons ? kToolbarButtonSize : kToolbarWideButtonWidth, kToolbarButtonHeight)))
            {
                editorState.GizmoSnapEnabled = !editorState.GizmoSnapEnabled;
            }

            ImGui::SameLine();

            if (DrawToolbarButton(
                    m_Fonts,
                    "ViewportFocus",
                    kIconFocus,
                    "Focus",
                    "Focus Selected (F)",
                    false,
                    true,
                    ImVec2(m_Fonts.HasIcons ? kToolbarButtonSize : kToolbarWideButtonWidth, kToolbarButtonHeight)))
            {
                editorState.RequestFocusSelected = true;
            }
        }
        ImGui::End();

        ImGui::PopStyleVar(3);
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
                    if (onlyVisible && !filter.PassFilter(entry.Text.c_str())) continue;
                    result += entry.Text;
                    if (result.empty() || result.back() != '\n')
                    {
                        result += '\n';
                    }
                }
                return result;
            };

            if (ImGui::Button("Clear")) consoleSink->Clear();

            ImGui::SameLine();
            if (ImGui::Button("Copy All"))
            {
                const std::string clipboardText = BuildClipboardText(false);
                ImGui::SetClipboardText(clipboardText.c_str());
            }

            ImGui::SameLine();
            if (ImGui::Button("Copy Visible"))
            {
                const std::string clipboardText = BuildClipboardText(true);
                ImGui::SetClipboardText(clipboardText.c_str());
            }

            ImGui::SameLine();
            ImGui::Checkbox("Auto-scroll", &autoScroll);

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
                    if (ImGui::Selectable("Clear")) consoleSink->Clear();
                    if (ImGui::Selectable("Copy All"))
                    {
                        const std::string clipboardText = BuildClipboardText(false);
                        ImGui::SetClipboardText(clipboardText.c_str());
                    }
                    if (ImGui::Selectable("Copy Visible"))
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
                            color = ImVec4(0.60f, 0.60f, 0.60f, 1.0f);
                            break;

                        case spdlog::level::info:
                            color = ImVec4(0.85f, 0.85f, 0.85f, 1.0f);
                            break;

                        case spdlog::level::warn:
                            color = ImVec4(1.00f, 0.78f, 0.20f, 1.0f);
                            break;

                        case spdlog::level::err:
                            color = ImVec4(1.00f, 0.35f, 0.35f, 1.0f);
                            break;

                        case spdlog::level::critical:
                            color = ImVec4(1.00f, 0.25f, 0.85f, 1.0f);
                            break;

                        default:
                            color = ImVec4(0.85f, 0.85f, 0.85f, 1.0f);
                            break;
                    }
                    ImGui::PushStyleColor(ImGuiCol_Text, color);
                    if (m_Fonts.Mono) ImGui::PushFont(m_Fonts.Mono);
                    ImGui::TextUnformatted(entry.Text.c_str());
                    if (m_Fonts.Mono) ImGui::PopFont();
                    ImGui::PopStyleColor();
                }
                if (autoScroll && wasAtBottom) ImGui::SetScrollHereY(1.0f);
            }
            ImGui::EndChild();
        }
        ImGui::End();
    }

    void ImGuiLayer::BuildTopbar(Editor::EditorActions& editorActions,
                                 Editor::EditorState& editorState)
    {
        const float topbarHeight = GetToolbarHeight();
        const ImGuiViewport* viewport = ImGui::GetMainViewport();

        ImGuiWindowFlags windowFlags =
            ImGuiWindowFlags_NoDocking |
            ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoNavFocus;

        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(ImVec2(viewport->WorkSize.x, topbarHeight));
        ImGui::SetNextWindowViewport(viewport->ID);

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.0f, 8.0f));

        ImGui::Begin("##MainTopbar", nullptr, windowFlags);
        ImGui::PopStyleVar(3);

        if (m_Fonts.Bold) ImGui::PushFont(m_Fonts.Bold);
        ImGui::TextUnformatted("QUIET");
        if (m_Fonts.Bold) ImGui::PopFont();

        ImGui::SameLine();
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);

        ImGui::SameLine();
        if (DrawToolbarButton(
                m_Fonts,
                "TopbarNew",
                kIconNew,
                "New",
                "New Scene"))
        {
            editorActions.Execute(Editor::EditorActionID::NewScene);
        }

        ImGui::SameLine();
        if (DrawToolbarButton(
                m_Fonts,
                "TopbarSave",
                kIconSave,
                "Save",
                "Save Scene (Ctrl+S)",
                false,
                editorActions.CanExecute(Editor::EditorActionID::SaveScene)))
        {
            editorActions.Execute(Editor::EditorActionID::SaveScene);
        }

        ImGui::SameLine();
        if (DrawToolbarButton(
                m_Fonts,
                "TopbarLoad",
                kIconLoad,
                "Load",
                "Load Scene",
                false,
                editorActions.CanExecute(Editor::EditorActionID::LoadScene)))
        {
            editorActions.Execute(Editor::EditorActionID::LoadScene);
        }

        ImGui::SameLine();
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);

        ImGui::SameLine();

        if (editorState.CurrentSceneMode == Editor::SceneMode::Edit)
        {
            if (DrawToolbarButton(
                    m_Fonts,
                    "TopbarSimulate",
                    kIconSimulate,
                    "Sim",
                    "Simulate Runtime (F6)",
                    false,
                    editorActions.CanExecute(Editor::EditorActionID::ToggleSimulate)))
            {
                editorActions.Execute(Editor::EditorActionID::ToggleSimulate);
            }

            ImGui::SameLine();

            if (DrawToolbarButton(
                    m_Fonts,
                    "TopbarPlay",
                    kIconPlay,
                    "Play",
                    "Play Game (F5)",
                    false,
                    editorActions.CanExecute(Editor::EditorActionID::TogglePlay)))
            {
                editorActions.Execute(Editor::EditorActionID::TogglePlay);
            }
        }
        else if (editorState.CurrentSceneMode == Editor::SceneMode::Simulate)
        {
            if (DrawToolbarButton(
                    m_Fonts,
                    "TopbarStopSimulate",
                    kIconStop,
                    "Stop",
                    "Stop Simulate (F6)",
                    false,
                    editorActions.CanExecute(Editor::EditorActionID::ToggleSimulate),
                    ImVec2(m_Fonts.HasIcons ? kToolbarButtonSize : kToolbarWideButtonWidth, kToolbarButtonHeight)))
            {
                editorActions.Execute(Editor::EditorActionID::ToggleSimulate);
            }
        }
        else if (editorState.CurrentSceneMode == Editor::SceneMode::Play)
        {
            if (DrawToolbarButton(
                    m_Fonts,
                    "TopbarStopPlay",
                    kIconStop,
                    "Stop",
                    "Stop Play (F5)",
                    false,
                    editorActions.CanExecute(Editor::EditorActionID::TogglePlay),
                    ImVec2(m_Fonts.HasIcons ? kToolbarButtonSize : kToolbarWideButtonWidth, kToolbarButtonHeight)))
            {
                editorActions.Execute(Editor::EditorActionID::TogglePlay);
            }
        }

        std::string statusText = GetSceneModeLabel(editorState.CurrentSceneMode);
        statusText += " ";
        statusText += editorState.SceneDisplayName;

        if (editorState.SceneDirty && editorState.CurrentSceneMode == Editor::SceneMode::Edit)
        {
            statusText += " *";
        }

        const float rightTextWidth = ImGui::CalcTextSize(statusText.c_str()).x;
        const float availableX = ImGui::GetWindowContentRegionMax().x;
        const float cursorX = availableX - rightTextWidth;

        if (cursorX > ImGui::GetCursorPosX())
        {
            ImGui::SameLine(cursorX);
        }
        else
        {
            ImGui::SameLine();
        }

        if (editorState.CurrentSceneMode == Editor::SceneMode::Play)
        {
            ImGui::TextColored(ImVec4(0.25f, 0.80f, 1.0f, 1.0f), "%s", statusText.c_str());
        }
        else if (editorState.CurrentSceneMode == Editor::SceneMode::Simulate)
        {
            ImGui::TextColored(ImVec4(0.30f, 0.85f, 0.35f, 1.0f), "%s", statusText.c_str());
        }
        else if (editorState.SceneDirty)
        {
            ImGui::TextColored(ImVec4(1.0f, 0.78f, 0.20f, 1.0f), "%s", statusText.c_str());
        }
        else
        {
            ImGui::TextUnformatted(statusText.c_str());
        }

        ImGui::End();
    }

    void ImGuiLayer::BuildViewport(Editor::EditorState &editorState,
                                   ImTextureID sceneViewportTexture,
                                   const std::function<void()> &viewportOverlayCallback)
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

                if (viewportOverlayCallback) viewportOverlayCallback();
            }
            else
            {
                editorState.ViewportImageHovered = false;
                ImGui::Text("Scene viewport unavailable.");
            }
        }
        ImGui::End();
    }

    void ImGuiLayer::BuildInspector(Scene::Scene& scene,
                                    Editor::EditorCommands& commands,
                                    Editor::EditorState& editorState,
                                    const MeshLibrary& meshLibrary)
    {
        ImGui::Begin("Inspector");
        {
            Scene::Actor* selectedActor = nullptr;
            if (editorState.SelectedActorID.has_value())
            {
                selectedActor = scene.FindActorByID(editorState.SelectedActorID.value());
                if (!selectedActor) editorState.SelectedActorID.reset();
            }

            if (!selectedActor)
            {
                ImGui::Text("No actor selected.");
                ImGui::End();
                return;
            }

            const bool isEditMode = editorState.CurrentSceneMode == Editor::SceneMode::Edit;
            auto& actor = *selectedActor;
            auto& transform = actor.Transform.WorldTransform;

            auto beginTrackedItem = [&]()
            {
                if (isEditMode && ImGui::IsItemActivated())
                {
                    commands.BeginInteraction(scene);
                }
            };

            auto endTrackedItem = [&]()
            {
                if (isEditMode && ImGui::IsItemDeactivated())
                {
                    commands.EndInteraction(scene);
                }
            };

            char nameBuffer[256]{};
            std::snprintf(nameBuffer, sizeof(nameBuffer), "%s", actor.Name.c_str());

            ImGui::Text("Actor");
            ImGui::Separator();

            if (!isEditMode)
            {
                ImGui::TextColored(
                    ImVec4(0.30f, 0.85f, 0.35f, 1.0f),
                    "Simulating runtime copy. Editing is disabled."
                );
                ImGui::Separator();
            }

            if (!isEditMode)
            {
                ImGui::BeginDisabled();
            }

            if (ImGui::InputText("Name", nameBuffer, sizeof(nameBuffer)))
            {
                commands.RenameActor(scene, actor.ID, nameBuffer);
            }
            beginTrackedItem();
            endTrackedItem();

            if (ImGui::DragFloat3("Position", &transform.Position.x, 0.1f))
            {
                commands.SetActorPosition(scene, actor.ID, transform.Position);
            }
            beginTrackedItem();
            endTrackedItem();

            if (ImGui::DragFloat3("Rotation", &transform.RotationDegrees.x, 0.5f))
            {
                commands.SetActorRotation(scene, actor.ID, transform.RotationDegrees);
            }
            beginTrackedItem();
            endTrackedItem();

            if (ImGui::DragFloat3("Scale", &transform.Scale.x, 0.05f, 0.01f, 100.0f))
            {
                commands.SetActorScale(scene, actor.ID, transform.Scale);
            }
            beginTrackedItem();
            endTrackedItem();

            ImGui::Separator();
            ImGui::Text("Components");

            if (!actor.StaticMesh.has_value())
            {
                if (ImGui::Button("Add Static Mesh"))
                {
                    commands.AddStaticMesh(scene, actor.ID, meshLibrary);
                }
            }
            else
            {
                if (ImGui::CollapsingHeader("Static Mesh", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    const auto names = meshLibrary.GetNames();

                    int currentIndex = 0;
                    for (int i = 0; i < static_cast<int>(names.size()); ++i)
                    {
                        if (names[i] == actor.StaticMesh->MeshResourceID)
                        {
                            currentIndex = i;
                            break;
                        }
                    }

                    const char* currentPreview =
                        names.empty() ? "<none>" : names[currentIndex].c_str();

                    if (ImGui::BeginCombo("Mesh Resource", currentPreview))
                    {
                        for (int i = 0; i < static_cast<int>(names.size()); ++i)
                        {
                            const bool isSelected = (i == currentIndex);
                            if (ImGui::Selectable(names[i].c_str(), isSelected))
                            {
                                commands.SetStaticMeshResource(scene, actor.ID, names[i], meshLibrary);
                            }
                            if (isSelected) ImGui::SetItemDefaultFocus();
                        }
                        ImGui::EndCombo();
                    }

                    bool visible = actor.StaticMesh->Visible;
                    if (ImGui::Checkbox("Visible", &visible))
                    {
                        commands.SetStaticMeshVisible(scene, actor.ID, visible);
                    }

                    if (ImGui::Button("Remove Static Mesh"))
                    {
                        commands.RemoveStaticMesh(scene, actor.ID);
                    }
                }
            }

            if (!actor.Rotator.has_value())
            {
                if (ImGui::Button("Add Rotator"))
                {
                    commands.AddRotator(scene, actor.ID);
                }
            }
            else
            {
                if (ImGui::CollapsingHeader("Rotator", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    if (ImGui::DragFloat("Rotation Speed",
                                         &actor.Rotator->RotationSpeedDegreesPerSecond,
                                         1.0f))
                    {
                        commands.SetRotatorSpeed(
                            scene,
                            actor.ID,
                            actor.Rotator->RotationSpeedDegreesPerSecond
                        );
                    }
                    beginTrackedItem();
                    endTrackedItem();

                    if (ImGui::Button("Remove Rotator"))
                    {
                        commands.RemoveRotator(scene, actor.ID);
                    }
                }
            }

            if (!isEditMode) ImGui::EndDisabled();
        }
        ImGui::End();
    }

    void ImGuiLayer::BuildHierarchy(Scene::Scene& scene,
                                    Editor::EditorActions& editorActions,
                                    Editor::EditorState& editorState)
    {
        ImGui::Begin("Scene");
        {
            const bool isEditMode = (editorState.CurrentSceneMode == Editor::SceneMode::Edit);
            DrawToolbarAction(editorActions, Editor::EditorActionID::CreateEmptyActor, "Create Empty");

            ImGui::Separator();

            std::optional<Editor::EditorActionID> pendingAction;
            std::optional<Core::UUID> pendingActorSelection;

            auto& actors = scene.GetActors();
            for (std::size_t i = 0; i < actors.size(); ++i)
            {
                const bool isSelected =
                    editorState.SelectedActorID.has_value() &&
                    editorState.SelectedActorID.value() == actors[i].ID;

                const std::string displayName = actors[i].Name.empty() ? "Unnamed Actor" : actors[i].Name;
                const std::string label = displayName + "##" + Core::UUIDToString(actors[i].ID);

                if (!isEditMode) ImGui::BeginDisabled();

                if (ImGui::Selectable(label.c_str(), isSelected) && isEditMode)
                {
                    editorState.SelectedActorID = actors[i].ID;
                }

                if (!isEditMode) ImGui::EndDisabled();

                if (ImGui::BeginPopupContextItem(label.c_str()))
                {
                    const bool canEdit =
                        editorState.CurrentSceneMode == Editor::SceneMode::Edit;

                    if (ImGui::MenuItem("Duplicate",
                                        editorActions.GetShortcut(Editor::EditorActionID::DuplicateSelected),
                                        false,
                                        canEdit))
                    {
                        pendingActorSelection = actors[i].ID;
                        pendingAction = Editor::EditorActionID::DuplicateSelected;
                    }

                    if (ImGui::MenuItem("Delete",
                                        editorActions.GetShortcut(Editor::EditorActionID::DeleteSelected),
                                        false,
                                        canEdit))
                    {
                        pendingActorSelection = actors[i].ID;
                        pendingAction = Editor::EditorActionID::DeleteSelected;
                    }

                    ImGui::EndPopup();
                }
            }

            if (pendingAction.has_value() && pendingActorSelection.has_value())
            {
                editorState.SelectedActorID = pendingActorSelection.value();
                editorActions.Execute(pendingAction.value());
            }
        }
        ImGui::End();
    }

    void ImGuiLayer::CreateDescriptorPool()
    {
        VkDescriptorPoolSize poolSizes[] =
        {
            { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
            { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
            { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
            { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
        };

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        poolInfo.maxSets = 1000 * static_cast<std::uint32_t>(std::size(poolSizes));
        poolInfo.poolSizeCount = static_cast<std::uint32_t>(std::size(poolSizes));
        poolInfo.pPoolSizes = poolSizes;

        if (vkCreateDescriptorPool(m_Device, &poolInfo, nullptr, &m_DescriptorPool) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create ImGui descriptor pool.");
        }
    }

    void ImGuiLayer::DestroyDescriptorPool()
    {
        if (m_DescriptorPool != VK_NULL_HANDLE)
        {
            vkDestroyDescriptorPool(m_Device, m_DescriptorPool, nullptr);
            m_DescriptorPool = VK_NULL_HANDLE;
        }
    }
}

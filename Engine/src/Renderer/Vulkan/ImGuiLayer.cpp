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
#include "Scene/Scene.h"

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
                                   Scene::Scene& scene,
                                   Editor::EditorState& editorState,
                                   const MeshLibrary& meshLibrary,
                                   ImTextureID sceneViewportTexture,
                                   const std::function<void()>& viewportOverlayCallback)
    {
        BuildTopbar(editorState);
        BuildDockspace();

        ImGui::Begin("Viewport");
        {
            editorState.ViewportFocused = ImGui::IsWindowFocused();
            editorState.ViewportHovered = ImGui::IsWindowHovered();

            const ImVec2 availableSize = ImGui::GetContentRegionAvail();

            editorState.ViewportWidth =
                availableSize.x > 1.0f ? static_cast<std::uint32_t>(availableSize.x) : 0;

            editorState.ViewportHeight =
                availableSize.y > 1.0f ? static_cast<std::uint32_t>(availableSize.y) : 0;

            editorState.ViewportImageHovered = false;

            if (sceneViewportTexture != 0 &&
                editorState.ViewportWidth > 0 &&
                editorState.ViewportHeight > 0)
            {
                ImGui::SetNextItemAllowOverlap();

                ImGui::Image(
                sceneViewportTexture,
                ImVec2(
                    static_cast<float>(editorState.ViewportWidth),
                    static_cast<float>(editorState.ViewportHeight)
                ),
                ImVec2(0.0f, 1.0f),
                ImVec2(1.0f, 0.0f)
            );

                const ImVec2 imageMin = ImGui::GetItemRectMin();
                const ImVec2 imageMax = ImGui::GetItemRectMax();

                editorState.ViewportScreenX = imageMin.x;
                editorState.ViewportScreenY = imageMin.y;
                editorState.ViewportWidth = static_cast<std::uint32_t>(imageMax.x - imageMin.x);
                editorState.ViewportHeight = static_cast<std::uint32_t>(imageMax.y - imageMin.y);
                editorState.ViewportImageHovered = ImGui::IsItemHovered();

                BuildViewportToolbar(editorState, imageMin);

                if (viewportOverlayCallback)
                {
                    viewportOverlayCallback();
                }
            }
            else
            {
                ImGui::Text("Scene viewport unavailable.");
            }
        }
        ImGui::End();

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
            ImGui::Text("Press Shift + F1 to toggle Cursor Capture");
            ImGui::Text("Mouse Capture: %s", stats.MouseCaptured ? "True" : "False");
            ImGui::Text("X: %.2f", stats.CameraPosition.x);
            ImGui::Text("Y: %.2f", stats.CameraPosition.y);
            ImGui::Text("Z: %.2f", stats.CameraPosition.z);

            ImGui::Separator();
            ImGui::Text("Tools:");
            ImGui::TextUnformatted("Shortcuts: T/R/S = gizmo, L = local/world, G = snap, F = focus");

        }
        ImGui::End();

        BuildInspector(scene, editorState, meshLibrary);
        BuildHierarchy(scene, editorState);
        BuildConsoleWindow();
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
        const float toolbarHeight = frameHeight + style.WindowPadding.y * 2.0f + 6.0f;

        return toolbarHeight;
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

    void ImGuiLayer::BuildViewportToolbar(Editor::EditorState &editorState,
                                          const ImVec2 &imageMin)
    {
        const float padding = 10.0f;
        const float height = ImGui::GetFrameHeight() + 16.0f;

        const ImVec2 toolbarPos(imageMin.x + padding, imageMin.y + padding);
        const ImVec2 toolbarSize(415.0f, height);

        ImGui::SetCursorScreenPos(toolbarPos);
        ImGui::SetNextWindowBgAlpha(0.88f);

        ImGuiWindowFlags flags =
            ImGuiWindowFlags_NoDecoration |
            ImGuiWindowFlags_NoDocking |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoNav |
            ImGuiWindowFlags_NoScrollbar;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 8.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6.0f, 6.0f));

        ImGui::BeginChild("##ViewportToolbar", toolbarSize, ImGuiChildFlags_Borders, flags);

        if (ImGui::Button("T"))
        {
            editorState.CurrentGizmoOperation = Editor::GizmoOperation::Translate;
        }
        ImGui::SameLine();

        if (ImGui::Button("R"))
        {
            editorState.CurrentGizmoOperation = Editor::GizmoOperation::Rotate;
        }
        ImGui::SameLine();

        if (ImGui::Button("S"))
        {
            editorState.CurrentGizmoOperation = Editor::GizmoOperation::Scale;
        }

        ImGui::SameLine();
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
        ImGui::SameLine();

        if (editorState.CurrentGizmoOperation != Editor::GizmoOperation::Scale)
        {
            if (ImGui::Button(
                    editorState.CurrentGizmoMode == Editor::GizmoMode::Local ? "Local" : "World"))
            {
                editorState.CurrentGizmoMode =
                    (editorState.CurrentGizmoMode == Editor::GizmoMode::Local)
                        ? Editor::GizmoMode::World
                        : Editor::GizmoMode::Local;
            }

            ImGui::SameLine();
        }

        if (ImGui::Button(editorState.GizmoSnapEnabled ? "Snap On" : "Snap Off"))
        {
            editorState.GizmoSnapEnabled = !editorState.GizmoSnapEnabled;
        }

        ImGui::SameLine();
        if (ImGui::Button("Focus"))
        {
            editorState.RequestFocusSelected = true;
        }

        ImGui::EndChild();
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

    void ImGuiLayer::BuildTopbar(Editor::EditorState& editorState)
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

        const bool isEditMode = editorState.CurrentSceneMode == Editor::SceneMode::Edit;

        if (!isEditMode) ImGui::BeginDisabled();

        ImGui::SameLine();
        if (ImGui::Button("New")) editorState.RequestNewScene = true;

        ImGui::SameLine();
        if (ImGui::Button("Save")) editorState.RequestSaveScene = true;

        ImGui::SameLine();
        if (ImGui::Button("Load")) editorState.RequestLoadScene = true;

        if (!isEditMode) ImGui::EndDisabled();

        ImGui::SameLine();
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);

        ImGui::SameLine();
        if (isEditMode)
        {
            if (ImGui::Button("Simulate"))
                editorState.RequestEnterSimulate = true;
        }
        else
        {
            if (ImGui::Button("Stop"))
                editorState.RequestStopSimulate = true;
        }

        std::string statusText = (editorState.CurrentSceneMode == Editor::SceneMode::Edit) ? "[EDIT] " : "[SIMULATE] ";
        statusText += editorState.SceneDisplayName;
        if (editorState.SceneDirty && editorState.CurrentSceneMode == Editor::SceneMode::Edit) statusText += " *";

        const float rightTextWidth = ImGui::CalcTextSize(statusText.c_str()).x;
        const float availableX = ImGui::GetWindowContentRegionMax().x;
        const float cursorX = availableX - rightTextWidth;

        if (cursorX > ImGui::GetCursorPosX()) ImGui::SameLine(cursorX);
        else ImGui::SameLine();

        if (editorState.CurrentSceneMode == Editor::SceneMode::Simulate)
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

    void ImGuiLayer::BuildInspector(Scene::Scene& scene,
                                Editor::EditorState& editorState,
                                const MeshLibrary& meshLibrary)
    {
        ImGui::Begin("Inspector");
        {
            Scene::Actor* selectedActor = nullptr;
            if (editorState.SelectedActorID.has_value())
            {
                selectedActor = scene.FindActorByID(editorState.SelectedActorID.value());
            }

            if (!selectedActor)
            {
                ImGui::Text("No actor selected.");
                ImGui::End();
                return;
            }

            const bool isEditMode = editorState.CurrentSceneMode == Editor::SceneMode::Edit;
            auto& actor = *selectedActor;

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

            if (!isEditMode) ImGui::BeginDisabled();
            if (ImGui::InputText("Name", nameBuffer, sizeof(nameBuffer)))
            {
                actor.Name = nameBuffer;
                editorState.SceneDirty = true;
            }

            auto& transform = actor.Transform.WorldTransform;

            if (ImGui::DragFloat3("Position", &transform.Position.x, 0.1f))
                editorState.SceneDirty = true;

            if (ImGui::DragFloat3("Rotation", &transform.RotationDegrees.x, 0.5f))
                editorState.SceneDirty = true;

            if (ImGui::DragFloat3("Scale", &transform.Scale.x, 0.05f, 0.01f, 100.0f))
                editorState.SceneDirty = true;

            ImGui::Separator();
            ImGui::Text("Components");

            if (!actor.StaticMesh.has_value())
            {
                if (ImGui::Button("Add Static Mesh"))
                {
                    const auto names = meshLibrary.GetNames();
                    if (!names.empty())
                    {
                        actor.StaticMesh = Scene::StaticMeshComponent{
                            names.front(),
                            meshLibrary.Get(names.front()),
                            true
                        };
                        editorState.SceneDirty = true;
                    }
                }
            }
            else
            {
                if (ImGui::CollapsingHeader("Static Mesh", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    auto names = meshLibrary.GetNames();

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
                                actor.StaticMesh->MeshResourceID = names[i];
                                actor.StaticMesh->Mesh = meshLibrary.Get(names[i]);
                                editorState.SceneDirty = true;
                            }

                            if (isSelected)
                            {
                                ImGui::SetItemDefaultFocus();
                            }
                        }

                        ImGui::EndCombo();
                    }

                    if (ImGui::Checkbox("Visible", &actor.StaticMesh->Visible))
                    {
                        editorState.SceneDirty = true;
                    }

                    if (ImGui::Button("Remove Static Mesh"))
                    {
                        actor.StaticMesh.reset();
                        editorState.SceneDirty = true;
                    }
                }
            }

            if (!actor.Rotator.has_value())
            {
                if (ImGui::Button("Add Rotator"))
                {
                    actor.Rotator = Scene::RotatorComponent{ 45.0f };
                    editorState.SceneDirty = true;
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
                        editorState.SceneDirty = true;
                    }

                    if (ImGui::Button("Remove Rotator"))
                    {
                        actor.Rotator.reset();
                        editorState.SceneDirty = true;
                    }
                }
            }

            if (!isEditMode) ImGui::EndDisabled();
        }
        ImGui::End();
    }

    void ImGuiLayer::BuildHierarchy(Scene::Scene& scene, Editor::EditorState& editorState)
    {
        ImGui::Begin("Scene Hierarchy");
        {
            const bool isEditMode = editorState.CurrentSceneMode == Editor::SceneMode::Edit;

            if (!isEditMode) ImGui::BeginDisabled();

            if (ImGui::Button("Create Empty"))
            {
                auto& actor = scene.CreateActor("Actor");
                editorState.SelectedActorID = actor.ID;
                editorState.SceneDirty = true;
            }

            ImGui::SameLine();

            const bool hasSelection = editorState.SelectedActorID.has_value();
            if (!hasSelection) ImGui::BeginDisabled();
            if (ImGui::Button("Duplicate") && hasSelection)
            {
                auto& duplicate = scene.DuplicateActor(editorState.SelectedActorID.value());
                editorState.SelectedActorID = duplicate.ID;
                editorState.SceneDirty = true;
            }
            if (!hasSelection) ImGui::EndDisabled();

            ImGui::SameLine();

            if (!hasSelection) ImGui::BeginDisabled();
            if (ImGui::Button("Delete") && hasSelection)
            {
                scene.DestroyActorByID(editorState.SelectedActorID.value());
                editorState.SelectedActorID.reset();
                editorState.SceneDirty = true;
            }
            if (!hasSelection) ImGui::EndDisabled();

            if (!isEditMode) ImGui::EndDisabled();

            ImGui::Separator();

            std::optional<Core::UUID> pendingDuplicate;
            std::optional<Core::UUID> pendingDelete;

            auto& actors = scene.GetActors();
            for (std::size_t i = 0; i < actors.size(); ++i)
            {
                const bool isSelected =
                    editorState.SelectedActorID.has_value() &&
                    editorState.SelectedActorID.value() == actors[i].ID;

                const std::string displayName =
                    actors[i].Name.empty() ? "Unnamed Actor" : actors[i].Name;

                const std::string label =
                    displayName + "##" + Core::UUIDToString(actors[i].ID);

                if (ImGui::Selectable(label.c_str(), isSelected))
                {
                    editorState.SelectedActorID = actors[i].ID;
                }

                if (ImGui::BeginPopupContextItem(label.c_str()))
                {
                    if (!isEditMode) ImGui::BeginDisabled();

                    if (ImGui::MenuItem("Duplicate"))
                    {
                        pendingDuplicate = actors[i].ID;
                    }

                    if (ImGui::MenuItem("Delete"))
                    {
                        pendingDelete = actors[i].ID;
                    }

                    if (!isEditMode) ImGui::EndDisabled();

                    ImGui::EndPopup();
                }
            }

            if (pendingDuplicate.has_value())
            {
                auto& duplicate = scene.DuplicateActor(pendingDuplicate.value());
                editorState.SelectedActorID = duplicate.ID;
                editorState.SceneDirty = true;
            }

            if (pendingDelete.has_value())
            {
                scene.DestroyActorByID(pendingDelete.value());

                if (editorState.SelectedActorID.has_value() &&
                    editorState.SelectedActorID.value() == pendingDelete.value())
                {
                    editorState.SelectedActorID.reset();
                }

                editorState.SceneDirty = true;
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

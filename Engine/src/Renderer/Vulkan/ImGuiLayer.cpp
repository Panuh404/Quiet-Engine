#include "ImGuiLayer.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_vulkan.h>
#include <ImGuizmo.h>

#include <GLFW/glfw3.h>
#include <stdexcept>

#include "ImGuiTheme.h"
#include "../../../include/Quiet/Editor/EditorState.h"
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
                                   ImTextureID sceneViewportTexture,
                                   const std::function<void()>& viewportOverlayCallback)
    {
        BuildTopbar();
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
            }
            else
            {
                auto& actor = *selectedActor;

                ImGui::Text("Actor");
                ImGui::Separator();

                char nameBuffer[256]{};
                std::snprintf(nameBuffer, sizeof(nameBuffer), "%s", actor.Name.c_str());

                if (ImGui::InputText("Name", nameBuffer, sizeof(nameBuffer)))
                {
                    actor.Name = nameBuffer;
                }

                ImGui::Separator();
                ImGui::Text("Transform");

                auto& transform = actor.Transform.WorldTransform;
                ImGui::DragFloat3("Position", &transform.Position.x, 0.1f);
                ImGui::DragFloat3("Rotation", &transform.RotationDegrees.x, 0.5f);
                ImGui::DragFloat3("Scale", &transform.Scale.x, 0.05f, 0.01f, 100.0f);

                ImGui::Separator();
                ImGui::Text("Components");
                ImGui::Text("StaticMesh: %s", (actor.StaticMesh.has_value() && actor.StaticMesh->IsValid()) ? "Yes" : "No");
                ImGui::Text("Rotator: %s", actor.Rotator.has_value() ? "Yes" : "No");

                if (actor.Rotator.has_value())
                {
                    ImGui::DragFloat("Rotation Speed", &actor.Rotator->RotationSpeedDegreesPerSecond, 1.0f);
                }
            }
        }
        ImGui::End();

        ImGui::Begin("Scene Hierarchy");
        {
            auto& actors = scene.GetActors();
            for (std::size_t i = 0; i < actors.size(); ++i)
            {
                const bool isSelected =
                    editorState.SelectedActorID.has_value() && editorState.SelectedActorID.value() == actors[i].ID;

                const char* actorName =
                    actors[i].Name.empty() ? "Unnamed Actor" : actors[i].Name.c_str();

                if (ImGui::Selectable(actorName, isSelected))
                {
                    editorState.SelectedActorID = actors[i].ID;
                }
            }
        }
        ImGui::End();
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

    void ImGuiLayer::BuildTopbar()
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

        if (m_Fonts.Bold)
            ImGui::PushFont(m_Fonts.Bold);

        ImGui::TextUnformatted("Quiet Editor");

        if (m_Fonts.Bold)
            ImGui::PopFont();

        ImGui::SameLine();
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
        ImGui::SameLine();

        ImGui::BeginDisabled();
        ImGui::Button("File");
        ImGui::SameLine();
        ImGui::Button("Edit");
        ImGui::SameLine();
        ImGui::Button("View");
        ImGui::SameLine();
        ImGui::Button("Tools");
        ImGui::EndDisabled();

        const char* statusText = "Placeholder Topbar";

        const float rightTextWidth = ImGui::CalcTextSize(statusText).x;
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

        ImGui::TextUnformatted(statusText);

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

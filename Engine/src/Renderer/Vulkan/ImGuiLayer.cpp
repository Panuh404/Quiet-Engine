#include "ImGuiLayer.h"

#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_vulkan.h>
#include <ImGuizmo.h>

#include <GLFW/glfw3.h>
#include <stdexcept>

#include "ImGuiTheme.h"
#include "../../../../Build/Intermediate/Release/_deps/imgui-src/imgui_internal.h"
#include "Quiet/Core/EditorState.h"
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

        SetupEditorFonts(
            io,
            "C:/Windows/Fonts/segoeui.ttf",
            28.0f
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
                                   Quiet::Scene::Scene& scene,
                                   Quiet::Core::EditorState& editorState,
                                   ImTextureID sceneViewportTexture,
                                   const std::function<void()>& viewportOverlayCallback)
    {
        BuildToolbar(editorState, scene);
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
            if (!editorState.SelectedActorIndex.has_value())
            {
                ImGui::Text("No actor selected.");
            }
            else if (editorState.SelectedActorIndex.value() >= scene.GetActors().size())
            {
                ImGui::Text("Selected actor index is invalid.");
                editorState.SelectedActorIndex.reset();
            }
            else
            {
                auto& actor = scene.GetActors()[editorState.SelectedActorIndex.value()];

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
                    editorState.SelectedActorIndex.has_value() && editorState.SelectedActorIndex.value() == i;

                const char* actorName =
                    actors[i].Name.empty() ? "Unnamed Actor" : actors[i].Name.c_str();

                if (ImGui::Selectable(actorName, isSelected))
                {
                    editorState.SelectedActorIndex = i;
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

    void ImGuiLayer::BuildToolbar(Quiet::Core::EditorState& editorState,
                              Quiet::Scene::Scene& scene)
    {
        const float toolbarHeight = GetToolbarHeight();
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
        ImGui::SetNextWindowSize(ImVec2(viewport->WorkSize.x, toolbarHeight));
        ImGui::SetNextWindowViewport(viewport->ID);

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 8.0f));

        ImGui::Begin("##MainToolbar", nullptr, windowFlags);
        {
            ImGui::PopStyleVar(3);

            if (ImGui::Button("Translate"))
                editorState.CurrentGizmoOperation = Quiet::Core::GizmoOperation::Translate;
            ImGui::SameLine();

            if (ImGui::Button("Rotate"))
                editorState.CurrentGizmoOperation = Quiet::Core::GizmoOperation::Rotate;
            ImGui::SameLine();

            if (ImGui::Button("Scale"))
                editorState.CurrentGizmoOperation = Quiet::Core::GizmoOperation::Scale;
            ImGui::SameLine();

            ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
            if (editorState.CurrentGizmoOperation != Quiet::Core::GizmoOperation::Scale)
            {
                ImGui::SameLine();
                if (ImGui::Button(
                        editorState.CurrentGizmoMode == Quiet::Core::GizmoMode::Local
                            ? "Local"
                            : "World"))
                {
                    editorState.CurrentGizmoMode =
                        (editorState.CurrentGizmoMode == Quiet::Core::GizmoMode::Local)
                            ? Quiet::Core::GizmoMode::World
                            : Quiet::Core::GizmoMode::Local;
                }
            }

            ImGui::SameLine();
            ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);

            ImGui::SameLine();

            if (ImGui::Button(editorState.GizmoSnapEnabled ? "Snap: On" : "Snap: Off"))
                editorState.GizmoSnapEnabled = !editorState.GizmoSnapEnabled;

            if (editorState.CurrentGizmoOperation == Quiet::Core::GizmoOperation::Translate)
            {
                ImGui::SameLine();
                ImGui::SetNextItemWidth(180.0f);
                ImGui::InputFloat3("##TranslateSnap", editorState.TranslateSnap);
            }
            else if (editorState.CurrentGizmoOperation == Quiet::Core::GizmoOperation::Rotate)
            {
                ImGui::SameLine();
                ImGui::SetNextItemWidth(90.0f);
                ImGui::InputFloat("##RotateSnap", &editorState.RotateSnap);
            }
            else if (editorState.CurrentGizmoOperation == Quiet::Core::GizmoOperation::Scale)
            {
                ImGui::SameLine();
                ImGui::SetNextItemWidth(90.0f);
                ImGui::InputFloat("##ScaleSnap", &editorState.ScaleSnap);
            }

            ImGui::SameLine();
            ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);

            ImGui::SameLine();

            if (ImGui::Button("Focus"))
                editorState.RequestFocusSelected = true;

            ImGui::SameLine();
            ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);

            ImGui::SameLine();

            if (editorState.SelectedActorIndex.has_value() &&
                editorState.SelectedActorIndex.value() < scene.GetActors().size())
            {
                const auto& actor = scene.GetActors()[editorState.SelectedActorIndex.value()];
                ImGui::Text("Selected: %s", actor.Name.c_str());
            }
            else
            {
                ImGui::TextUnformatted("Selected: None");
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
}

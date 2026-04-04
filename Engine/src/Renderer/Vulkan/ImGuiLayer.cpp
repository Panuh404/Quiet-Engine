#include "ImGuiLayer.h"

#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_vulkan.h>
#include <ImGuizmo.h>

#include <GLFW/glfw3.h>
#include <stdexcept>

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

        ImGui::StyleColorsDark();
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

        ImGui::Begin("Quiet Debug");
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

                ImGui::Separator();
                ImGui::Text("Gizmo");

                const bool translateSelected = editorState.CurrentGizmoOperation == Core::GizmoOperation::Translate;
                const bool rotateSelected = editorState.CurrentGizmoOperation == Core::GizmoOperation::Rotate;
                const bool scaleSelected = editorState.CurrentGizmoOperation == Core::GizmoOperation::Scale;

                if (ImGui::RadioButton("Translate", translateSelected))
                {
                    editorState.CurrentGizmoOperation = Core::GizmoOperation::Translate;
                }
                ImGui::SameLine();
                if (ImGui::RadioButton("Rotate", rotateSelected))
                {
                    editorState.CurrentGizmoOperation = Core::GizmoOperation::Rotate;
                }
                ImGui::SameLine();
                if (ImGui::RadioButton("Scale", scaleSelected))
                {
                    editorState.CurrentGizmoOperation = Core::GizmoOperation::Scale;
                }

                if (editorState.CurrentGizmoOperation != Core::GizmoOperation::Scale)
                {
                    const bool localSelected = editorState.CurrentGizmoMode == Core::GizmoMode::Local;
                    const bool worldSelected = editorState.CurrentGizmoMode == Core::GizmoMode::World;

                    if (ImGui::RadioButton("Local", localSelected))
                    {
                        editorState.CurrentGizmoMode = Core::GizmoMode::Local;
                    }
                    ImGui::SameLine();
                    if (ImGui::RadioButton("World", worldSelected))
                    {
                        editorState.CurrentGizmoMode = Core::GizmoMode::World;
                    }
                }

                ImGui::Checkbox("Snap", &editorState.GizmoSnapEnabled);
                switch (editorState.CurrentGizmoOperation)
                {
                    case Core::GizmoOperation::Translate:
                        ImGui::InputFloat3("Translate Snap", editorState.TranslateSnap);
                        break;

                    case Core::GizmoOperation::Rotate:
                        ImGui::InputFloat("Rotate Snap", &editorState.RotateSnap);
                        break;

                    case Core::GizmoOperation::Scale:
                        ImGui::InputFloat("Scale Snap", &editorState.ScaleSnap);
                        break;
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
        ImGuiDockNodeFlags dockspaceFlags = ImGuiDockNodeFlags_PassthruCentralNode;
        ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport(), dockspaceFlags);
    }
}

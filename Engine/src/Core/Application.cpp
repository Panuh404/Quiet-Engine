#include "Quiet/Core/Log.h"
#include "Quiet/Core/Application.h"
#include "Quiet/Platform/Window.h"
#include "Scene/FreeCamera.h"

#include <stdexcept>
#include <utility>
#include <limits>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "imgui.h"
#include "ImGuizmo.h"
#include "GLFW/glfw3.h"
#include "Input/Input.h"

#include "Renderer/Vulkan/VulkanInstance.h"
#include "Renderer/Vulkan/VulkanSurface.h"
#include "Renderer/Vulkan/PhysicalDevice.h"
#include "Renderer/Vulkan/DepthResources.h"
#include "Renderer/Vulkan/FrameSync.h"
#include "Renderer/Vulkan/GraphicsCommands.h"
#include "Renderer/Vulkan/LogicalDevice.h"
#include "Renderer/Vulkan/Swapchain.h"
#include "Renderer/Vulkan/StaticMeshPipeline.h"
#include "Renderer/Vulkan/CameraResources.h"
#include "Renderer/Vulkan/ImGuiLayer.h"
#include "Renderer/Vulkan/MeshLibrary.h"
#include "Renderer/Vulkan/RenderTypes.h"
#include "Renderer/Vulkan/SceneViewportTarget.h"
#include "Renderer/Vulkan/StaticMesh.h"
#include "Renderer/Vulkan/StaticMeshRenderSystem.h"
#include "Scene/Scene.h"


namespace
{
    struct Ray
    {
        glm::vec3 Origin{ 0.0f };
        glm::vec3 Direction{ 0.0f, 0.0f, -1.0f };
    };

    struct AABB
    {
        glm::vec3 Min{ 0.0f };
        glm::vec3 Max{ 0.0f };
    };

    AABB TransformAABB(const glm::mat4& transform, const glm::vec3& localMin, const glm::vec3& localMax)
    {
        const glm::vec3 corners[8] =
        {
            { localMin.x, localMin.y, localMin.z },
            { localMax.x, localMin.y, localMin.z },
            { localMin.x, localMax.y, localMin.z },
            { localMax.x, localMax.y, localMin.z },
            { localMin.x, localMin.y, localMax.z },
            { localMax.x, localMin.y, localMax.z },
            { localMin.x, localMax.y, localMax.z },
            { localMax.x, localMax.y, localMax.z }
        };

        glm::vec3 worldMin(std::numeric_limits<float>::max());
        glm::vec3 worldMax(std::numeric_limits<float>::lowest());

        for (const glm::vec3& corner : corners)
        {
            const glm::vec4 transformed = transform * glm::vec4(corner, 1.0f);
            const glm::vec3 p = glm::vec3(transformed);

            worldMin = glm::min(worldMin, p);
            worldMax = glm::max(worldMax, p);
        }

        return { worldMin, worldMax };
    }

    bool IntersectRayAABB(const Ray& ray, const AABB& box, float& hitDistance)
    {
        float tMin = 0.0f;
        float tMax = std::numeric_limits<float>::max();

        for (int axis = 0; axis < 3; ++axis)
        {
            const float origin = ray.Origin[axis];
            const float direction = ray.Direction[axis];
            const float minValue = box.Min[axis];
            const float maxValue = box.Max[axis];

            if (std::abs(direction) < 0.000001f)
            {
                if (origin < minValue || origin > maxValue)
                {
                    return false;
                }
                continue;
            }

            const float invDir = 1.0f / direction;
            float t0 = (minValue - origin) * invDir;
            float t1 = (maxValue - origin) * invDir;

            if (t0 > t1)
            {
                std::swap(t0, t1);
            }

            tMin = std::max(tMin, t0);
            tMax = std::min(tMax, t1);

            if (tMax < tMin)
            {
                return false;
            }
        }

        hitDistance = tMin;
        return true;
    }

    void TransitionImageLayout(VkCommandBuffer commandBuffer,
                               VkImage image,
                               VkImageAspectFlags aspectMask,
                               VkImageLayout oldLayout,
                               VkImageLayout newLayout)
    {
        if (oldLayout == newLayout) return;

        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = oldLayout;
        barrier.newLayout = newLayout;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = image;
        barrier.subresourceRange.aspectMask = aspectMask;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;

        VkPipelineStageFlags srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        VkPipelineStageFlags dstStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;

        if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED &&
            newLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
        {
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

            srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            dstStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        }
        else if (oldLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL &&
                 newLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
        {
            barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
            barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

            srcStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
            dstStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        }
        else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED &&
                 newLayout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL)
        {
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                                    VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

            srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            dstStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                       VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        }
        else if (oldLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL &&
                 newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
        {
            barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

            srcStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        }
        else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED &&
                 newLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR)
        {
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = 0;

            srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            dstStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        }
        else if (oldLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR &&
                 newLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
        {
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
                                    VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

            srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            dstStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        }
        else if (oldLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL &&
                 newLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR)
        {
            barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            barrier.dstAccessMask = 0;

            srcStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            dstStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        }
        else
        {
            throw std::runtime_error("Unsupported image layout transition.");
        }

        vkCmdPipelineBarrier(
            commandBuffer, srcStage, dstStage, 0, 0, nullptr,
            0, nullptr, 1, &barrier
        );
    }

    ImGuizmo::OPERATION ToImGuizmoOperation(Quiet::Core::GizmoOperation operation)
    {
        switch (operation)
        {
            case Quiet::Core::GizmoOperation::Translate: return ImGuizmo::TRANSLATE;
            case Quiet::Core::GizmoOperation::Rotate:    return ImGuizmo::ROTATE;
            case Quiet::Core::GizmoOperation::Scale:     return ImGuizmo::SCALE;
            default:                                     return ImGuizmo::TRANSLATE;
        }
    }

    ImGuizmo::MODE ToImGuizmoMode(Quiet::Core::GizmoMode mode)
    {
        switch (mode)
        {
            case Quiet::Core::GizmoMode::Local: return ImGuizmo::LOCAL;
            case Quiet::Core::GizmoMode::World: return ImGuizmo::WORLD;
            default:                            return ImGuizmo::WORLD;
        }
    }
}

namespace Quiet::Core
{
    Application::Application(AppSpec spec) : m_Spec(std::move(spec))
    {
        Log::Init();

        Quiet::Platform::WindowDesc windowDesc{};
        windowDesc.Width = m_Spec.Width;
        windowDesc.Height = m_Spec.Height;
        windowDesc.Title = m_Spec.Name;

        m_Window = std::make_unique<Quiet::Platform::Window>(windowDesc);
        if (!m_Window->IsVulkanSupported())
        {
            QUIET_CORE_ERROR("GLFW reports that Vulkan is not available.");
            throw std::runtime_error(
                "GLFW reports that Vulkan is not available. Check your Vulkan loader, drivers, or SDK."
            );
        }
        m_Input = std::make_unique<Quiet::Input::Input>(*m_Window);
        SetMouseCaptureEnabled(false);

        m_FreeCamera = std::make_unique<Quiet::Scene::FreeCamera>();
        m_Scene = std::make_unique<Quiet::Scene::Scene>();

        m_VulkanInstance = std::make_unique<Quiet::Renderer::Vulkan::VulkanInstance>(
            m_Spec.Name,
            m_Window->GetRequiredVulkanInstanceExtensions());

        m_VulkanSurface = std::make_unique<Quiet::Renderer::Vulkan::VulkanSurface>(
            m_VulkanInstance->GetHandle(),
            *m_Window);

        m_PhysicalDevice = std::make_unique<Quiet::Renderer::Vulkan::PhysicalDevice>(
            m_VulkanInstance->GetHandle(),
            m_VulkanSurface->GetHandle(),
            m_VulkanInstance->GetApiVersion());

        m_LogicalDevice = std::make_unique<Quiet::Renderer::Vulkan::LogicalDevice>(
            *m_PhysicalDevice);

        m_Swapchain = std::make_unique<Quiet::Renderer::Vulkan::Swapchain>(
            *m_Window,
            *m_PhysicalDevice,
            *m_LogicalDevice,
            *m_VulkanSurface);

        m_DepthResources = std::make_unique<Quiet::Renderer::Vulkan::DepthResources>(
            *m_PhysicalDevice,
            *m_LogicalDevice,
            *m_Swapchain);

        m_SceneViewportTarget = std::make_unique<Quiet::Renderer::Vulkan::SceneViewportTarget>(
            m_PhysicalDevice->GetHandle(),
            m_LogicalDevice->GetHandle(),
            m_Swapchain->GetImageFormat(),
            m_DepthResources->GetFormat());

        m_GraphicsCommands = std::make_unique<Quiet::Renderer::Vulkan::GraphicsCommands>(
            *m_PhysicalDevice,
            *m_LogicalDevice,
            *m_Swapchain);

        m_ImGuiLayer = std::make_unique<Quiet::Renderer::Vulkan::ImGuiLayer>(
            static_cast<GLFWwindow*>(m_Window->GetNativeHandle()),
            m_VulkanInstance->GetHandle(),
            m_PhysicalDevice->GetHandle(),
            m_LogicalDevice->GetHandle(),
            m_PhysicalDevice->GetQueueFamilies().GraphicsFamily.value(),
            m_LogicalDevice->GetGraphicsQueue(),
            2,
            static_cast<std::uint32_t>(m_Swapchain->GetImages().size()),
            m_Swapchain->GetImageFormat(),
            m_DepthResources->GetFormat()
        );

        m_MeshLibrary = std::make_unique<Quiet::Renderer::Vulkan::MeshLibrary>();
        m_MeshLibrary->CreateQuad("Quad", *m_PhysicalDevice, *m_LogicalDevice, *m_GraphicsCommands);
        m_MeshLibrary->CreateCube("Cube", *m_PhysicalDevice, *m_LogicalDevice, *m_GraphicsCommands);

        m_CameraResources = std::make_unique<Quiet::Renderer::Vulkan::CameraResources>(
            *m_PhysicalDevice,
            *m_LogicalDevice,
            *m_Swapchain
        );

        m_StaticMeshPipeline = std::make_unique<Quiet::Renderer::Vulkan::StaticMeshPipeline>(
            *m_LogicalDevice,
            *m_Swapchain,
            *m_DepthResources,
            m_CameraResources->GetDescriptorSetLayout());

        m_FrameSync = std::make_unique<Quiet::Renderer::Vulkan::FrameSync>(
            *m_LogicalDevice,
            *m_Swapchain);

        m_StaticMeshRenderSystem = std::make_unique<Quiet::Renderer::Vulkan::StaticMeshRenderSystem>();

        auto& actors = m_Scene->GetActors();
        if (actors.size() >= 3)
        {
            actors[0].StaticMesh = Quiet::Scene::StaticMeshComponent{ m_MeshLibrary->Get("Cube"), true };
            actors[1].StaticMesh = Quiet::Scene::StaticMeshComponent{ m_MeshLibrary->Get("Quad"), true };
            actors[2].StaticMesh = Quiet::Scene::StaticMeshComponent{ m_MeshLibrary->Get("Cube"), true };

            actors[0].Transform.WorldTransform.Position = { 0.0f, 0.0f, 0.0f };
            actors[1].Transform.WorldTransform.Position = { -2.0f, 0.0f, 0.0f };
            actors[2].Transform.WorldTransform.Position = { 2.0f, 0.0f, -1.0f };
        }
        QUIET_CORE_INFO("Assigned mesh resources to {} actors.", m_Scene->GetActors().size());
    }

    Application::~Application()
    {
        if (m_LogicalDevice)
        {
            vkDeviceWaitIdle(m_LogicalDevice->GetHandle());
        }

        m_Scene.reset();
        m_FrameSync.reset();
        m_StaticMeshRenderSystem.reset();
        m_GraphicsCommands.reset();
        m_StaticMeshPipeline.reset();
        m_CameraResources.reset();
        m_SceneViewportTarget.reset();
        m_ImGuiLayer.reset();
        m_DepthResources.reset();
        m_MeshLibrary.reset();
        m_Swapchain.reset();
        m_LogicalDevice.reset();
        m_PhysicalDevice.reset();
        m_VulkanSurface.reset();
        m_VulkanInstance.reset();
        m_FreeCamera.reset();
        m_Window.reset();
        Log::Shutdown();
    }

    int Application::Run()
    {
        QUIET_CORE_INFO("Starting application: {}", m_Spec.Name);

        // deltaTime
        double previousTime = glfwGetTime();

        while (m_Running && !m_Window->ShouldClose())
        {
            m_Window->PollEvents();
            m_Input->Update();

            const bool shiftDown =
                m_Input->IsKeyDown(Input::KeyCode::LeftShift) ||
                m_Input->IsKeyDown(Input::KeyCode::RightShift);

            if (shiftDown && m_Input->IsKeyPressed(Input::KeyCode::F1))
            {
                ToggleMouseCapture();
            }

            const double currentTime = glfwGetTime();
            const float deltaTime = static_cast<float>(currentTime - previousTime);
            previousTime = currentTime;

            if (m_ViewportResizePending && m_PendingViewportWidth > 0 && m_PendingViewportHeight > 0)
            {
                vkDeviceWaitIdle(m_LogicalDevice->GetHandle());

                m_SceneViewportTarget->Resize(
                    m_PendingViewportWidth,
                    m_PendingViewportHeight
                );

                m_ViewportResizePending = false;
                m_SceneViewportReady = false;
            }

            if (m_MouseCaptureEnabled)
            {
                m_FreeCamera->Update(*m_Input, deltaTime);
            }
            m_Scene->Update(deltaTime);

            m_ImGuiLayer->BeginFrame();
            {
                Quiet::Renderer::Vulkan::ImGuiStats stats{};
                stats.FrameTimeMs = deltaTime * 1000.0f;
                stats.FPS = (deltaTime > 0.0f) ? (1.0f / deltaTime) : 0.0f;
                stats.ActorCount = static_cast<std::uint32_t>(m_Scene->GetActors().size());
                stats.MeshCount = static_cast<std::uint32_t>(m_MeshLibrary->GetCount());
                stats.SwapchainExtent = m_Swapchain->GetExtent();
                stats.MouseCaptured = m_MouseCaptureEnabled;
                stats.CameraPosition = m_FreeCamera->GetPosition();

                ImTextureID sceneViewportTexture = 0;
                if (m_SceneViewportTarget &&
                    m_SceneViewportTarget->IsValid() &&
                    m_SceneViewportReady)
                {
                    sceneViewportTexture = m_SceneViewportTarget->GetImGuiTextureID();
                }

                m_ImGuiLayer->BuildEditorUI(
                    stats,
                    *m_Scene,
                    m_EditorState,
                    sceneViewportTexture,
                    [this]()
                    {
                        TrySelectActorFromViewport();
                        BuildSelectedActorGizmo();
                    }
                );

                if (m_EditorState.ViewportWidth > 0 && m_EditorState.ViewportHeight > 0)
                {
                    const VkExtent2D currentExtent =
                        (m_SceneViewportTarget && m_SceneViewportTarget->IsValid())
                            ? m_SceneViewportTarget->GetExtent()
                            : VkExtent2D{ 0, 0 };

                    if (!m_SceneViewportTarget->IsValid() ||
                        currentExtent.width != m_EditorState.ViewportWidth ||
                        currentExtent.height != m_EditorState.ViewportHeight)
                    {
                        m_PendingViewportWidth = m_EditorState.ViewportWidth;
                        m_PendingViewportHeight = m_EditorState.ViewportHeight;
                        m_ViewportResizePending = true;
                    }
                }
            }
            m_ImGuiLayer->EndFrame();

            const bool frameCompleted = DrawFrame();
            if (frameCompleted && (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable))
            {
                ImGui::UpdatePlatformWindows();
                ImGui::RenderPlatformWindowsDefault();
            }
        }

        QUIET_CORE_INFO("Shutting down.");
        return 0;
    }

    bool Application::DrawFrame()
    {
        if (m_Window->WasFramebufferResized())
        {
            RecreateSwapchain();
            return false;
        }

        const VkDevice device = m_LogicalDevice->GetHandle();
        const VkFence inFlightFence = m_FrameSync->GetInFlightFence();

        if (vkWaitForFences(device, 1, &inFlightFence, VK_TRUE, UINT64_MAX) != VK_SUCCESS)
            throw std::runtime_error("Failed to wait for in-flight fence.");

        if (vkResetFences(device, 1, &inFlightFence) != VK_SUCCESS)
            throw std::runtime_error("Failed to reset in-flight fence.");

        std::uint32_t imageIndex = 0;
        const VkResult acquireResult = vkAcquireNextImageKHR(
            device,
            m_Swapchain->GetHandle(),
            UINT64_MAX,
            m_FrameSync->GetImageAvailableSemaphore(),
            VK_NULL_HANDLE,
            &imageIndex
        );

        if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR)
        {
            RecreateSwapchain();
            return false;
        }
        if (acquireResult != VK_SUCCESS && acquireResult != VK_SUBOPTIMAL_KHR)
            throw std::runtime_error("Failed to acquire swapchain image.");

        const VkExtent2D sceneExtent =
            (m_SceneViewportTarget && m_SceneViewportTarget->IsValid())
                ? m_SceneViewportTarget->GetExtent()
                : m_Swapchain->GetExtent();

        const float aspect =
            static_cast<float>(sceneExtent.width) /
            static_cast<float>(sceneExtent.height);

        const auto camera = m_FreeCamera->BuildUniformData(aspect, false);
        m_CameraResources->Update(imageIndex, camera);

        RecordFrameCommandBuffer(imageIndex);

        const VkCommandBuffer commandBuffer = m_GraphicsCommands->GetCommandBuffer(imageIndex);
        const VkSemaphore waitSemaphores[] = { m_FrameSync->GetImageAvailableSemaphore() };
        const VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT  };
        const VkSemaphore signalSemaphores[] = { m_FrameSync->GetRenderFinishedSemaphore(imageIndex) };

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = waitSemaphores;
        submitInfo.pWaitDstStageMask = waitStages;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = signalSemaphores;

        if (vkQueueSubmit(m_LogicalDevice->GetGraphicsQueue(), 1, &submitInfo, inFlightFence) != VK_SUCCESS)
            throw std::runtime_error("Failed to submit graphics command buffer.");

        const VkSwapchainKHR swapchains[] = { m_Swapchain->GetHandle() };

        VkPresentInfoKHR presentInfo{};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = signalSemaphores;
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = swapchains;
        presentInfo.pImageIndices = &imageIndex;

        const VkResult presentResult = vkQueuePresentKHR(m_LogicalDevice->GetPresentQueue(), &presentInfo);
        if (presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR || m_Window->WasFramebufferResized())
        {
            RecreateSwapchain();
            return false;
        }

        if (presentResult != VK_SUCCESS)
            throw std::runtime_error("Failed to present swapchain image.");

        return true;
    }

    void Application::RecreateSwapchain()
    {
        auto [width, height] = m_Window->GetFramebufferSize();
        while (width == 0 || height == 0)
        {
            m_Window->WaitEvents();
            std::tie(width, height) = m_Window->GetFramebufferSize();
        }

        if (vkDeviceWaitIdle(m_LogicalDevice->GetHandle()) != VK_SUCCESS)
            throw std::runtime_error("Failed to wait for device idle during swapchain recreation.");

        m_SceneViewportTarget.reset();
        m_ImGuiLayer.reset();
        m_FrameSync.reset();
        m_GraphicsCommands.reset();
        m_StaticMeshPipeline.reset();
        m_CameraResources.reset();
        m_DepthResources.reset();

        auto oldSwapchain = std::move(m_Swapchain);

        m_Swapchain = std::make_unique<Quiet::Renderer::Vulkan::Swapchain>(
            *m_Window,
            *m_PhysicalDevice,
            *m_LogicalDevice,
            *m_VulkanSurface,
            oldSwapchain ? oldSwapchain->GetHandle() : VK_NULL_HANDLE
        );

        m_DepthResources = std::make_unique<Quiet::Renderer::Vulkan::DepthResources>(
            *m_PhysicalDevice,
            *m_LogicalDevice,
            *m_Swapchain
        );

        m_CameraResources = std::make_unique<Quiet::Renderer::Vulkan::CameraResources>(
            *m_PhysicalDevice,
            *m_LogicalDevice,
            *m_Swapchain);

        m_StaticMeshPipeline = std::make_unique<Quiet::Renderer::Vulkan::StaticMeshPipeline>(
            *m_LogicalDevice,
            *m_Swapchain,
            *m_DepthResources,
            m_CameraResources->GetDescriptorSetLayout()
        );

        m_GraphicsCommands = std::make_unique<Quiet::Renderer::Vulkan::GraphicsCommands>(
            *m_PhysicalDevice,
            *m_LogicalDevice,
            *m_Swapchain
        );

        m_FrameSync = std::make_unique<Quiet::Renderer::Vulkan::FrameSync>(
            *m_LogicalDevice,
            *m_Swapchain
        );

        m_ImGuiLayer = std::make_unique<Quiet::Renderer::Vulkan::ImGuiLayer>(
        static_cast<GLFWwindow*>(m_Window->GetNativeHandle()),
            m_VulkanInstance->GetHandle(),
            m_PhysicalDevice->GetHandle(),
            m_LogicalDevice->GetHandle(),
            m_PhysicalDevice->GetQueueFamilies().GraphicsFamily.value(),
            m_LogicalDevice->GetGraphicsQueue(),
            2,
            static_cast<std::uint32_t>(m_Swapchain->GetImages().size()),
            m_Swapchain->GetImageFormat(),
            m_DepthResources->GetFormat()
        );

        m_SceneViewportTarget = std::make_unique<Quiet::Renderer::Vulkan::SceneViewportTarget>(
            m_PhysicalDevice->GetHandle(),
            m_LogicalDevice->GetHandle(),
            m_Swapchain->GetImageFormat(),
            m_DepthResources->GetFormat());

        m_SceneViewportReady = false;
        m_ViewportResizePending = false;
        m_PendingViewportWidth = 0;
        m_PendingViewportHeight = 0;

        oldSwapchain.reset();
        m_Window->ResetFramebufferResizedFlag();
        QUIET_CORE_INFO("Recreated swapchain-dependent resources.");
    }

    void Application::SetMouseCaptureEnabled(bool enabled)
    {
        m_MouseCaptureEnabled = enabled;

        m_Input->SetCursorDisabled(enabled);
        m_Input->EnableRawMouseMotionIfSupported(enabled);
        m_Input->ResetMouseDelta();

        QUIET_CORE_TRACE(
            "Mouse capture: {}. Toggle with Shift+F1.",
            enabled ? "enabled" : "disabled"
        );
    }

    void Application::ToggleMouseCapture()
    {
        SetMouseCaptureEnabled(!m_MouseCaptureEnabled);
    }

   void Application::BuildSelectedActorGizmo()
    {
        if (m_MouseCaptureEnabled) return;
        if (!m_SceneViewportTarget || !m_SceneViewportTarget->IsValid() || !m_SceneViewportReady) return;
        if (!m_EditorState.SelectedActorIndex.has_value()) return;

        auto& actors = m_Scene->GetActors();
        const std::size_t selectedIndex = m_EditorState.SelectedActorIndex.value();

        if (selectedIndex >= actors.size())
        {
            m_EditorState.SelectedActorIndex.reset();
            return;
        }

        if (m_EditorState.ViewportWidth == 0 || m_EditorState.ViewportHeight == 0) return;
        if (!m_EditorState.ViewportImageHovered && !ImGuizmo::IsUsing()) return;

        if (ImGui::IsKeyPressed(ImGuiKey_T)) { m_EditorState.CurrentGizmoOperation = GizmoOperation::Translate; }
        if (ImGui::IsKeyPressed(ImGuiKey_R)) { m_EditorState.CurrentGizmoOperation = GizmoOperation::Rotate; }
        if (ImGui::IsKeyPressed(ImGuiKey_S)) { m_EditorState.CurrentGizmoOperation = GizmoOperation::Scale; }

        const float aspect =
            static_cast<float>(m_SceneViewportTarget->GetExtent().width) /
            static_cast<float>(m_SceneViewportTarget->GetExtent().height);

        const auto cameraData = m_FreeCamera->BuildUniformData(aspect, false);

        auto& actor = actors[selectedIndex];
        auto& transform = actor.Transform.WorldTransform;

        float translation[3] =
        {
            transform.Position.x,
            transform.Position.y,
            transform.Position.z
        };

        float rotation[3] =
        {
            transform.RotationDegrees.x,
            transform.RotationDegrees.y,
            transform.RotationDegrees.z
        };

        float scale[3] =
        {
            transform.Scale.x,
            transform.Scale.y,
            transform.Scale.z
        };

        float modelArray[16]{};
        ImGuizmo::RecomposeMatrixFromComponents(
            translation,
            rotation,
            scale,
            modelArray
        );

        glm::mat4 model = glm::make_mat4(modelArray);

        ImGuizmo::SetOrthographic(false);
        ImGuizmo::SetDrawlist();
        ImGuizmo::SetRect(
            m_EditorState.ViewportScreenX,
            m_EditorState.ViewportScreenY,
            static_cast<float>(m_EditorState.ViewportWidth),
            static_cast<float>(m_EditorState.ViewportHeight)
        );

        const ImGuizmo::OPERATION operation = ToImGuizmoOperation(m_EditorState.CurrentGizmoOperation);
        const ImGuizmo::MODE mode = ToImGuizmoMode(m_EditorState.CurrentGizmoMode);

        float snapValues[3] = { 0.0f, 0.0f, 0.0f };
        const float* snapPointer = nullptr;

        if (m_EditorState.GizmoSnapEnabled)
        {
            switch (m_EditorState.CurrentGizmoOperation)
            {
            case GizmoOperation::Translate:
                snapValues[0] = m_EditorState.TranslateSnap[0];
                snapValues[1] = m_EditorState.TranslateSnap[1];
                snapValues[2] = m_EditorState.TranslateSnap[2];
                snapPointer = snapValues;
                break;

            case GizmoOperation::Rotate:
                snapValues[0] = m_EditorState.RotateSnap;
                snapPointer = snapValues;
                break;

            case GizmoOperation::Scale:
                snapValues[0] = m_EditorState.ScaleSnap;
                snapPointer = snapValues;
                break;
            }
        }

        const bool changed = ImGuizmo::Manipulate(
            glm::value_ptr(cameraData.View),
            glm::value_ptr(cameraData.Projection),
            operation,
            mode,
            glm::value_ptr(model),
            nullptr,
            snapPointer
        );

        if (changed || ImGuizmo::IsUsing())
        {
            float translation[3]{};
            float rotation[3]{};
            float scale[3]{};

            ImGuizmo::DecomposeMatrixToComponents(glm::value_ptr(model), translation, rotation, scale);

            transform.Position = { translation[0], translation[1], translation[2] };
            transform.RotationDegrees = { rotation[0], rotation[1], rotation[2] };
            transform.Scale = { scale[0], scale[1], scale[2] };
        }
    }

    void Application::TrySelectActorFromViewport()
    {
        if (m_MouseCaptureEnabled) return;
        if (!m_EditorState.ViewportImageHovered) return;
        if (!m_Input->IsMouseButtonPressed(Quiet::Input::MouseCode::ButtonLeft)) return;
        if (ImGuizmo::IsOver() || ImGuizmo::IsUsing()) return;

        Ray ray{};
        if (!BuildViewportPickRay(ray)) return;

        auto& actors = m_Scene->GetActors();
        float closestHit = std::numeric_limits<float>::max();
        std::optional<std::size_t> bestHit;

        for (std::size_t i = 0; i < actors.size(); ++i)
        {
            auto& actor = actors[i];

            if (!actor.StaticMesh.has_value() ||
                !actor.StaticMesh->IsValid() ||
                !actor.StaticMesh->Visible)
                continue;

            const auto& mesh = actor.StaticMesh->Mesh;
            const glm::mat4 model = actor.Transform.WorldTransform.ToMatrix();

            const AABB worldBounds = TransformAABB(
                model,
                mesh->GetLocalBoundsMin(),
                mesh->GetLocalBoundsMax()
            );

            float hitDistance = 0.0f;
            if (IntersectRayAABB(ray, worldBounds, hitDistance))
            {
                if (hitDistance < closestHit)
                {
                    closestHit = hitDistance;
                    bestHit = i;
                }
            }
        }

        m_EditorState.SelectedActorIndex = bestHit;
    }

    bool Application::BuildViewportPickRay(Ray& outRay) const
    {
        if (!m_SceneViewportTarget || !m_SceneViewportTarget->IsValid())
            return false;

        if (m_EditorState.ViewportWidth == 0 || m_EditorState.ViewportHeight == 0)
            return false;

        const auto [mouseX, mouseY] = m_Input->GetMousePosition();

        const float localX = static_cast<float>(mouseX) - m_EditorState.ViewportScreenX;
        const float localY = static_cast<float>(mouseY) - m_EditorState.ViewportScreenY;

        if (localX < 0.0f || localY < 0.0f ||
            localX > static_cast<float>(m_EditorState.ViewportWidth) ||
            localY > static_cast<float>(m_EditorState.ViewportHeight))
            return false;

        const float viewportHeight = static_cast<float>(m_EditorState.ViewportHeight);
        const float flippedY = viewportHeight - localY;

        const float aspect =
            static_cast<float>(m_SceneViewportTarget->GetExtent().width) /
            static_cast<float>(m_SceneViewportTarget->GetExtent().height);

        const auto cameraData = m_FreeCamera->BuildUniformData(aspect, false);

        const glm::vec4 viewport(
            0.0f,
            0.0f,
            static_cast<float>(m_EditorState.ViewportWidth),
            static_cast<float>(m_EditorState.ViewportHeight)
        );

        const glm::vec3 nearPoint = glm::unProjectZO(
            glm::vec3(localX, flippedY, 0.0f),
            cameraData.View,
            cameraData.Projection,
            viewport
        );

        const glm::vec3 farPoint = glm::unProjectZO(
            glm::vec3(localX, flippedY, 1.0f),
            cameraData.View,
            cameraData.Projection,
            viewport
        );

        outRay.Origin = nearPoint;
        outRay.Direction = glm::normalize(farPoint - nearPoint);
        return true;
    }

   void Application::RecordFrameCommandBuffer(std::uint32_t imageIndex)
    {
        const VkCommandBuffer commandBuffer = m_GraphicsCommands->GetCommandBuffer(imageIndex);

        vkResetCommandBuffer(commandBuffer, 0);

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS)
            throw std::runtime_error("Failed to begin graphics command buffer.");

        const VkImage swapchainImage = m_Swapchain->GetImages().at(imageIndex);
        const VkImageView swapchainImageView = m_Swapchain->GetImageViews().at(imageIndex);

        // PASS A: Render scene to offscreen SceneViewportTarget
        if (m_SceneViewportTarget && m_SceneViewportTarget->IsValid())
        {
            TransitionImageLayout(
                commandBuffer,
                m_SceneViewportTarget->GetColorImage(),
                VK_IMAGE_ASPECT_COLOR_BIT,
                m_SceneViewportTarget->GetColorLayout(),
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
            );
            m_SceneViewportTarget->SetColorLayout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

            TransitionImageLayout(
                commandBuffer,
                m_SceneViewportTarget->GetDepthImage(),
                VK_IMAGE_ASPECT_DEPTH_BIT,
                m_SceneViewportTarget->GetDepthLayout(),
                VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL
            );
            m_SceneViewportTarget->SetDepthLayout(VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

            VkClearValue sceneColorClear{};
            sceneColorClear.color = { { 0.05f, 0.07f, 0.12f, 1.0f } };

            VkClearValue sceneDepthClear{};
            sceneDepthClear.depthStencil = { 1.0f, 0 };

            VkRenderingAttachmentInfo sceneColorAttachment{};
            sceneColorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
            sceneColorAttachment.imageView = m_SceneViewportTarget->GetColorImageView();
            sceneColorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            sceneColorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            sceneColorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            sceneColorAttachment.clearValue = sceneColorClear;

            VkRenderingAttachmentInfo sceneDepthAttachment{};
            sceneDepthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
            sceneDepthAttachment.imageView = m_SceneViewportTarget->GetDepthImageView();
            sceneDepthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
            sceneDepthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            sceneDepthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            sceneDepthAttachment.clearValue = sceneDepthClear;

            VkRenderingInfo sceneRenderingInfo{};
            sceneRenderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
            sceneRenderingInfo.renderArea.offset = { 0, 0 };
            sceneRenderingInfo.renderArea.extent = m_SceneViewportTarget->GetExtent();
            sceneRenderingInfo.layerCount = 1;
            sceneRenderingInfo.colorAttachmentCount = 1;
            sceneRenderingInfo.pColorAttachments = &sceneColorAttachment;
            sceneRenderingInfo.pDepthAttachment = &sceneDepthAttachment;
            sceneRenderingInfo.pStencilAttachment = nullptr;

            vkCmdBeginRendering(commandBuffer, &sceneRenderingInfo);

            Quiet::Renderer::Vulkan::StaticMeshRenderContext renderContext{};
            renderContext.CommandBuffer = commandBuffer;
            renderContext.ImageIndex = imageIndex;
            renderContext.Pipeline = m_StaticMeshPipeline.get();
            renderContext.Camera = m_CameraResources.get();
            renderContext.SceneData = m_Scene.get();
            renderContext.Extent = m_SceneViewportTarget->GetExtent();

            m_StaticMeshRenderSystem->Record(renderContext);

            vkCmdEndRendering(commandBuffer);

            TransitionImageLayout(
                commandBuffer,
                m_SceneViewportTarget->GetColorImage(),
                VK_IMAGE_ASPECT_COLOR_BIT,
                m_SceneViewportTarget->GetColorLayout(),
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            );
            m_SceneViewportTarget->SetColorLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            m_SceneViewportReady = true;
        }

        // PASS B: Render ImGui to swapchain
        TransitionImageLayout(
            commandBuffer,
            swapchainImage,
            VK_IMAGE_ASPECT_COLOR_BIT,
            m_Swapchain->GetTrackedLayout(imageIndex),
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
        );

        VkClearValue uiClear{};
        uiClear.color = { { 0.10f, 0.10f, 0.10f, 1.0f } };

        VkRenderingAttachmentInfo uiColorAttachment{};
        uiColorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        uiColorAttachment.imageView = swapchainImageView;
        uiColorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        uiColorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        uiColorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        uiColorAttachment.clearValue = uiClear;

        VkRenderingInfo uiRenderingInfo{};
        uiRenderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
        uiRenderingInfo.renderArea.offset = { 0, 0 };
        uiRenderingInfo.renderArea.extent = m_Swapchain->GetExtent();
        uiRenderingInfo.layerCount = 1;
        uiRenderingInfo.colorAttachmentCount = 1;
        uiRenderingInfo.pColorAttachments = &uiColorAttachment;
        uiRenderingInfo.pDepthAttachment = nullptr;
        uiRenderingInfo.pStencilAttachment = nullptr;

        vkCmdBeginRendering(commandBuffer, &uiRenderingInfo);
        m_ImGuiLayer->Render(commandBuffer);
        vkCmdEndRendering(commandBuffer);

        TransitionImageLayout(
            commandBuffer,
            swapchainImage,
            VK_IMAGE_ASPECT_COLOR_BIT,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
        );

        if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS)
            throw std::runtime_error("Failed to end graphics command buffer.");

        m_Swapchain->SetTrackedLayout(imageIndex, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
    }
}

#include "VulkanRenderer.h"

#include "Quiet/Core/AppSpec.h"
#include "Quiet/Editor/EditorState.h"
#include "Quiet/Core/Log.h"
#include "Quiet/Platform/Window.h"
#include "Scene/FreeCamera.h"
#include "Scene/Scene.h"

#include <stdexcept>

#include "GLFW/glfw3.h"
#include "Renderer/Vulkan/VulkanInstance.h"
#include "Renderer/Vulkan/VulkanSurface.h"
#include "Renderer/Vulkan/PhysicalDevice.h"
#include "Renderer/Vulkan/LogicalDevice.h"
#include "Renderer/Vulkan/Swapchain.h"
#include "Renderer/Vulkan/DepthResources.h"
#include "Renderer/Vulkan/SceneViewportTarget.h"
#include "Renderer/Vulkan/GraphicsCommands.h"
#include "Renderer/Vulkan/MeshLibrary.h"
#include "Renderer/Vulkan/CameraResources.h"
#include "Renderer/Vulkan/StaticMeshPipeline.h"
#include "Renderer/Vulkan/FrameSync.h"
#include "Renderer/Vulkan/StaticMeshRenderSystem.h"

namespace
{
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
}

namespace Quiet::Renderer::Vulkan
{
    VulkanRenderer::VulkanRenderer(const Core::AppSpec &spec,
                                   Platform::Window &window)
        : m_Window(&window)
    {
        CreateRenderResources(window, spec);
    }

    VulkanRenderer::~VulkanRenderer()
    {
        DestroyRenderResources();
    }

    void VulkanRenderer::CreateRenderResources(Platform::Window &window, const Core::AppSpec &spec)
    {
        m_VulkanInstance = std::make_unique<VulkanInstance>(
            spec.Name,
            window.GetRequiredVulkanInstanceExtensions());

        m_VulkanSurface = std::make_unique<VulkanSurface>(
            m_VulkanInstance->GetHandle(),
            window);

        m_PhysicalDevice = std::make_unique<PhysicalDevice>(
            m_VulkanInstance->GetHandle(),
            m_VulkanSurface->GetHandle(),
            m_VulkanInstance->GetApiVersion());

        m_LogicalDevice = std::make_unique<LogicalDevice>(*m_PhysicalDevice);

        m_Swapchain = std::make_unique<Swapchain>(
            window,
            *m_PhysicalDevice,
            *m_LogicalDevice,
            *m_VulkanSurface);

        m_DepthResources = std::make_unique<DepthResources>(
            *m_PhysicalDevice,
            *m_LogicalDevice,
            *m_Swapchain);

        m_SceneViewportTarget = std::make_unique<SceneViewportTarget>(
            m_PhysicalDevice->GetHandle(),
            m_LogicalDevice->GetHandle(),
            m_Swapchain->GetImageFormat(),
            m_DepthResources->GetFormat());

        m_GraphicsCommands = std::make_unique<GraphicsCommands>(
            *m_PhysicalDevice,
            *m_LogicalDevice,
            *m_Swapchain);

        m_ImGuiLayer = std::make_unique<ImGuiLayer>(
            static_cast<GLFWwindow*>(window.GetNativeHandle()),
            m_VulkanInstance->GetHandle(),
            m_PhysicalDevice->GetHandle(),
            m_LogicalDevice->GetHandle(),
            m_PhysicalDevice->GetQueueFamilies().GraphicsFamily.value(),
            m_LogicalDevice->GetGraphicsQueue(),
            2,
            static_cast<std::uint32_t>(m_Swapchain->GetImages().size()),
            m_Swapchain->GetImageFormat(),
            m_DepthResources->GetFormat());

        m_MeshLibrary = std::make_unique<MeshLibrary>();
        m_MeshLibrary->CreateQuad("Quad", *m_PhysicalDevice, *m_LogicalDevice, *m_GraphicsCommands);
        m_MeshLibrary->CreateCube("Cube", *m_PhysicalDevice, *m_LogicalDevice, *m_GraphicsCommands);

        m_CameraResources = std::make_unique<CameraResources>(
            *m_PhysicalDevice,
            *m_LogicalDevice,
            *m_Swapchain);

        m_StaticMeshPipeline = std::make_unique<StaticMeshPipeline>(
            *m_LogicalDevice,
            *m_Swapchain,
            *m_DepthResources,
            m_CameraResources->GetDescriptorSetLayout());

        m_FrameSync = std::make_unique<FrameSync>(
            *m_LogicalDevice,
            *m_Swapchain);

        m_StaticMeshRenderSystem = std::make_unique<StaticMeshRenderSystem>();
    }

    void VulkanRenderer::DestroyRenderResources()
    {
        if (m_LogicalDevice)
        {
            vkDeviceWaitIdle(m_LogicalDevice->GetHandle());
        }

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
    }

    bool VulkanRenderer::DrawFrame(Scene::Scene &scene,
                                   Scene::FreeCamera &camera,
                                   ImGuiLayer &imguiLayer,
                                   Editor::EditorState &editorState)
    {
        if (m_Window->WasFramebufferResized())
        {
            RecreateSwapchain();
            return false;
        }

        VkDevice device = m_LogicalDevice->GetHandle();
        VkFence inFlightFence = m_FrameSync->GetInFlightFence();

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

        auto cameraData = camera.BuildUniformData(aspect, false);
        m_CameraResources->Update(imageIndex, cameraData);

        RecordFrameCommandBuffer(imageIndex, scene, camera, editorState, imguiLayer);

        VkCommandBuffer commandBuffer = m_GraphicsCommands->GetCommandBuffer(imageIndex);
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

    void VulkanRenderer::RecreateSwapchain()
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

        m_Swapchain = std::make_unique<Swapchain>(
            *m_Window,
            *m_PhysicalDevice,
            *m_LogicalDevice,
            *m_VulkanSurface,
            oldSwapchain ? oldSwapchain->GetHandle() : VK_NULL_HANDLE
        );

        m_DepthResources = std::make_unique<DepthResources>(
            *m_PhysicalDevice,
            *m_LogicalDevice,
            *m_Swapchain
        );

        m_CameraResources = std::make_unique<CameraResources>(
            *m_PhysicalDevice,
            *m_LogicalDevice,
            *m_Swapchain);

        m_StaticMeshPipeline = std::make_unique<StaticMeshPipeline>(
            *m_LogicalDevice,
            *m_Swapchain,
            *m_DepthResources,
            m_CameraResources->GetDescriptorSetLayout()
        );

        m_GraphicsCommands = std::make_unique<GraphicsCommands>(
            *m_PhysicalDevice,
            *m_LogicalDevice,
            *m_Swapchain
        );

        m_FrameSync = std::make_unique<FrameSync>(
            *m_LogicalDevice,
            *m_Swapchain
        );

        m_ImGuiLayer = std::make_unique<ImGuiLayer>(
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

        m_SceneViewportTarget = std::make_unique<SceneViewportTarget>(
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

    void VulkanRenderer::ApplyPendingViewportResize()
    {
        EnsureSceneViewportSize();
    }

    ImTextureID VulkanRenderer::GetSceneViewportTextureID() const
    {
        if (!m_SceneViewportTarget ||
            !m_SceneViewportTarget->IsValid() ||
            !m_SceneViewportReady) return 0;
        return m_SceneViewportTarget->GetImGuiTextureID();
    }

    VkExtent2D VulkanRenderer::GetSwapchainExtent() const
    {
        return m_Swapchain->GetExtent();
    }

    VkExtent2D VulkanRenderer::GetSceneViewportExtent() const
    {
        if (m_SceneViewportTarget &&
            m_SceneViewportTarget->IsValid()) return m_SceneViewportTarget->GetExtent();
        return m_Swapchain->GetExtent();
    }

    void VulkanRenderer::RequestViewportResize(std::uint32_t width, std::uint32_t height)
    {
        m_PendingViewportWidth = width;
        m_PendingViewportHeight = height;
        m_ViewportResizePending = true;
    }

    void VulkanRenderer::EnsureSceneViewportSize()
    {
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
    }

    void VulkanRenderer::RecordFrameCommandBuffer(std::uint32_t imageIndex,
                                                  Scene::Scene &scene,
                                                  Scene::FreeCamera &camera,
                                                  Editor::EditorState &editorState,
                                                  ImGuiLayer& imguiLayer)
    {
        VkCommandBuffer commandBuffer = m_GraphicsCommands->GetCommandBuffer(imageIndex);
        vkResetCommandBuffer(commandBuffer, 0);

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS)
            throw std::runtime_error("Failed to begin graphics command buffer.");

        VkImage swapchainImage = m_Swapchain->GetImages().at(imageIndex);
        VkImageView swapchainImageView = m_Swapchain->GetImageViews().at(imageIndex);

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

            StaticMeshRenderContext renderContext{};
            renderContext.CommandBuffer = commandBuffer;
            renderContext.ImageIndex = imageIndex;
            renderContext.Pipeline = m_StaticMeshPipeline.get();
            renderContext.Camera = m_CameraResources.get();
            renderContext.SceneData = &scene;
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
        imguiLayer.Render(commandBuffer);
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

#include "LauncherRenderer.h"

#include <stdexcept>

#include "GLFW/glfw3.h"

#include "Quiet/Platform/Window.h"

#include "Renderer/UI/ImGuiLayer.h"

#include "Renderer/Vulkan/Context/GraphicsCommands.h"
#include "Renderer/Vulkan/Context/LogicalDevice.h"
#include "Renderer/Vulkan/Context/PhysicalDevice.h"
#include "Renderer/Vulkan/Context/VulkanInstance.h"
#include "Renderer/Vulkan/Context/VulkanSurface.h"

#include "Renderer/Vulkan/Frame/FrameSync.h"
#include "Renderer/Vulkan/Frame/Swapchain.h"

namespace
{
    void TransitionSwapchainImage(VkCommandBuffer commandBuffer, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout)
    {
        if (oldLayout == newLayout) return;

        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = oldLayout;
        barrier.newLayout = newLayout;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = image;

        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;

        VkPipelineStageFlags sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        VkPipelineStageFlags destinationStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;

        if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
        {
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            destinationStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        }
        else if (oldLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR && newLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
        {
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            destinationStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        }
        else if (oldLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR)
        {
            barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            barrier.dstAccessMask = 0;
            sourceStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            destinationStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        }
        else
        {
            throw std::runtime_error("Unsupported launcher swapchain image layout transition.");
        }

        vkCmdPipelineBarrier(
            commandBuffer,
            sourceStage,
            destinationStage,
            0,
            0,
            nullptr,
            0,
            nullptr,
            1,
            &barrier
        );
    }
}

namespace Quiet::Launcher
{
    LauncherRenderer::LauncherRenderer(Platform::Window& window) : m_Window(&window)
    {
        CreateResources();
    }

    LauncherRenderer::~LauncherRenderer()
    {
        DestroyResources();
    }

    void LauncherRenderer::CreateResources()
    {
        m_VulkanInstance = std::make_unique<Renderer::Vulkan::VulkanInstance>(
            "Quiet Launcher",
            m_Window->GetRequiredVulkanInstanceExtensions());

        m_VulkanSurface = std::make_unique<Renderer::Vulkan::VulkanSurface>(
            m_VulkanInstance->GetHandle(),
            *m_Window);

        m_PhysicalDevice = std::make_unique<Renderer::Vulkan::PhysicalDevice>(
            m_VulkanInstance->GetHandle(),
            m_VulkanSurface->GetHandle(),
            m_VulkanInstance->GetApiVersion());

        m_LogicalDevice = std::make_unique<Renderer::Vulkan::LogicalDevice>(
            *m_PhysicalDevice);

        m_Swapchain = std::make_unique<Renderer::Vulkan::Swapchain>(
            *m_Window,
            *m_PhysicalDevice,
            *m_LogicalDevice,
            *m_VulkanSurface);

        m_GraphicsCommands = std::make_unique<Renderer::Vulkan::GraphicsCommands>(
            *m_PhysicalDevice,
            *m_LogicalDevice,
            *m_Swapchain);

        m_FrameSync = std::make_unique<Renderer::Vulkan::FrameSync>(
            *m_LogicalDevice,
            *m_Swapchain);

        m_ImGuiLayer = std::make_unique<Renderer::UI::ImGuiLayer>(
            static_cast<GLFWwindow*>(m_Window->GetNativeHandle()),
            m_VulkanInstance->GetHandle(),
            m_PhysicalDevice->GetHandle(),
            m_LogicalDevice->GetHandle(),
            m_PhysicalDevice->GetQueueFamilies().GraphicsFamily.value(),
            m_LogicalDevice->GetGraphicsQueue(),
            2,
            static_cast<std::uint32_t>(m_Swapchain->GetImages().size()),
            m_Swapchain->GetImageFormat(),
            VK_FORMAT_UNDEFINED,
            "Quiet_Launcher.ini",
            false,
            false);
    }

    void LauncherRenderer::DestroyResources()
    {
        WaitIdle();

        m_ImGuiLayer.reset();
        m_FrameSync.reset();
        m_GraphicsCommands.reset();
        m_Swapchain.reset();
        m_LogicalDevice.reset();
        m_PhysicalDevice.reset();
        m_VulkanSurface.reset();
        m_VulkanInstance.reset();
    }

    Renderer::UI::ImGuiLayer& LauncherRenderer::GetImGuiLayer()
    {
        return *m_ImGuiLayer;
    }

    void LauncherRenderer::WaitIdle() const
    {
        if (m_LogicalDevice)
        {
            vkDeviceWaitIdle(m_LogicalDevice->GetHandle());
        }
    }

    bool LauncherRenderer::DrawFrame()
    {
        if (m_Window->WasFramebufferResized())
        {
            RecreateSwapchain();
            return false;
        }

        const VkDevice device = m_LogicalDevice->GetHandle();
        const VkFence inFlightFence = m_FrameSync->GetInFlightFence();

        if (vkWaitForFences(device, 1, &inFlightFence, VK_TRUE, UINT64_MAX) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to wait for launcher frame fence.");
        }

        if (vkResetFences(device, 1, &inFlightFence) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to reset launcher frame fence.");
        }

        std::uint32_t imageIndex = 0;

        const VkResult acquireResult = vkAcquireNextImageKHR(
            device,
            m_Swapchain->GetHandle(),
            UINT64_MAX,
            m_FrameSync->GetImageAvailableSemaphore(),
            VK_NULL_HANDLE,
            &imageIndex);

        if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR)
        {
            RecreateSwapchain();
            return false;
        }

        if (acquireResult != VK_SUCCESS && acquireResult != VK_SUBOPTIMAL_KHR)
        {
            throw std::runtime_error( "Failed to acquire launcher swapchain image.");
        }

        RecordCommandBuffer(imageIndex);
        VkCommandBuffer commandBuffer = m_GraphicsCommands->GetCommandBuffer(imageIndex);

        const VkSemaphore waitSemaphores[] =
        {
            m_FrameSync->GetImageAvailableSemaphore()
        };

        const VkPipelineStageFlags waitStages[] =
        {
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
        };

        const VkSemaphore signalSemaphores[] =
        {
            m_FrameSync->GetRenderFinishedSemaphore(imageIndex)
        };

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
        {
            throw std::runtime_error("Failed to submit launcher frame.");
        }

        const VkSwapchainKHR swapchains[] =
        {
            m_Swapchain->GetHandle()
        };

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
        {
            throw std::runtime_error("Failed to present launcher frame.");
        }

        return true;
    }

    void LauncherRenderer::RecordCommandBuffer(std::uint32_t imageIndex)
    {
        VkCommandBuffer commandBuffer = m_GraphicsCommands->GetCommandBuffer(imageIndex);
        vkResetCommandBuffer(commandBuffer, 0);

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

        if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to begin launcher command buffer.");
        }

        VkImage swapchainImage = m_Swapchain->GetImages().at(imageIndex);
        VkImageView swapchainImageView = m_Swapchain->GetImageViews().at(imageIndex);

        TransitionSwapchainImage(
            commandBuffer,
            swapchainImage,
            m_Swapchain->GetTrackedLayout(imageIndex),
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

        VkClearValue clearValue{};
        clearValue.color = { { 0.010f, 0.012f, 0.017f, 1.0f } };

        VkRenderingAttachmentInfo colorAttachment{};
        colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        colorAttachment.imageView = swapchainImageView;
        colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.clearValue = clearValue;

        VkRenderingInfo renderingInfo{};
        renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
        renderingInfo.renderArea.offset = { 0, 0 };
        renderingInfo.renderArea.extent = m_Swapchain->GetExtent();
        renderingInfo.layerCount = 1;
        renderingInfo.colorAttachmentCount = 1;
        renderingInfo.pColorAttachments = &colorAttachment;

        vkCmdBeginRendering(commandBuffer, &renderingInfo);
        m_ImGuiLayer->Render(commandBuffer);
        vkCmdEndRendering(commandBuffer);

        TransitionSwapchainImage(commandBuffer, swapchainImage, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

        if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to end launcher command buffer.");
        }

        m_Swapchain->SetTrackedLayout(imageIndex, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
    }

    void LauncherRenderer::RecreateSwapchain()
    {
        auto [width, height] = m_Window->GetFramebufferSize();
        while (width == 0 || height == 0)
        {
            m_Window->WaitEvents();
            const auto framebufferSize = m_Window->GetFramebufferSize();

            width = framebufferSize.first;
            height = framebufferSize.second;
        }

        WaitIdle();

        m_ImGuiLayer.reset();
        m_FrameSync.reset();
        m_GraphicsCommands.reset();

        auto oldSwapchain = std::move(m_Swapchain);

        m_Swapchain = std::make_unique<Renderer::Vulkan::Swapchain>(
            *m_Window,
            *m_PhysicalDevice,
            *m_LogicalDevice,
            *m_VulkanSurface,
            oldSwapchain ? oldSwapchain->GetHandle() : VK_NULL_HANDLE);

        m_GraphicsCommands = std::make_unique<Renderer::Vulkan::GraphicsCommands>(
            *m_PhysicalDevice,
            *m_LogicalDevice,
            *m_Swapchain);

        m_FrameSync = std::make_unique<Renderer::Vulkan::FrameSync>(
            *m_LogicalDevice,
            *m_Swapchain);

        m_ImGuiLayer = std::make_unique<Renderer::UI::ImGuiLayer>(
            static_cast<GLFWwindow*>(m_Window->GetNativeHandle()),
            m_VulkanInstance->GetHandle(),
            m_PhysicalDevice->GetHandle(),
            m_LogicalDevice->GetHandle(),
            m_PhysicalDevice->GetQueueFamilies().GraphicsFamily.value(),
            m_LogicalDevice->GetGraphicsQueue(),
            2,
            static_cast<std::uint32_t>(m_Swapchain->GetImages().size()),
            m_Swapchain->GetImageFormat(),
            VK_FORMAT_UNDEFINED,
            "Quiet_Launcher.ini",
            false,
            false);

        oldSwapchain.reset();
        m_Window->ResetFramebufferResizedFlag();
    }
}
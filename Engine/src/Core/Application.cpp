#include "Quiet/Core/Log.h"
#include "Quiet/Core/Application.h"
#include "Quiet/Platform/Window.h"
#include "Renderer/Vulkan/VulkanInstance.h"
#include "Renderer/Vulkan/VulkanSurface.h"
#include "Renderer/Vulkan/PhysicalDevice.h"

#include <stdexcept>
#include <utility>

#include "Renderer/Vulkan/DepthResources.h"
#include "Renderer/Vulkan/FrameSync.h"
#include "Renderer/Vulkan/GraphicsCommands.h"
#include "Renderer/Vulkan/LogicalDevice.h"
#include "Renderer/Vulkan/Swapchain.h"
#include "Renderer/Vulkan/TriangleMesh.h"
#include "Renderer/Vulkan/TrianglePipeline.h"

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

        m_GraphicsCommands = std::make_unique<Quiet::Renderer::Vulkan::GraphicsCommands>(
            *m_PhysicalDevice,
            *m_LogicalDevice,
            *m_Swapchain);

        m_TriangleMesh = std::make_unique<Quiet::Renderer::Vulkan::TriangleMesh>(
            *m_PhysicalDevice,
            *m_LogicalDevice,
            *m_GraphicsCommands);

        m_TrianglePipeline = std::make_unique<Quiet::Renderer::Vulkan::TrianglePipeline>(
            *m_LogicalDevice,
            *m_Swapchain,
            *m_DepthResources);

        m_FrameSync = std::make_unique<Quiet::Renderer::Vulkan::FrameSync>(
            *m_LogicalDevice,
            *m_Swapchain);
    }

    Application::~Application()
    {
        if (m_LogicalDevice)
        {
            vkDeviceWaitIdle(m_LogicalDevice->GetHandle());
        }

        m_FrameSync.reset();
        m_GraphicsCommands.reset();
        m_TrianglePipeline.reset();
        m_TriangleMesh.reset();
        m_DepthResources.reset();
        m_Swapchain.reset();
        m_LogicalDevice.reset();
        m_PhysicalDevice.reset();
        m_VulkanSurface.reset();
        m_VulkanInstance.reset();
        m_Window.reset();
        Log::Shutdown();
    }

    int Application::Run()
    {
        QUIET_CORE_INFO("Starting application: {}", m_Spec.Name);

        while (m_Running && !m_Window->ShouldClose())
        {
            m_Window->PollEvents();
            DrawFrame();
        }

        QUIET_CORE_INFO("Shutting down.");
        return 0;
    }

    void Application::DrawFrame()
    {
        if (m_Window->WasFramebufferResized())
        {
            RecreateSwapchain();
            return;
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
            return;
        }
        if (acquireResult != VK_SUCCESS && acquireResult != VK_SUBOPTIMAL_KHR)
            throw std::runtime_error("Failed to acquire swapchain image.");

        //RecordClearCommandBuffer(imageIndex);
        RecordTriangleCommandBuffer(imageIndex);

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
            return;
        }

        if (presentResult != VK_SUCCESS)
            throw std::runtime_error("Failed to present swapchain image.");
    }

    void Application::RecordClearCommandBuffer(std::uint32_t imageIndex)
    {
        const VkCommandBuffer commandBuffer = m_GraphicsCommands->GetCommandBuffer(imageIndex);

        if (vkResetCommandBuffer(commandBuffer, 0) != VK_SUCCESS)
            throw std::runtime_error("Failed to reset graphics command buffer.");

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

        if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS)
            throw std::runtime_error("Failed to begin graphics command buffer.");

        const VkImage image = m_Swapchain->GetImages().at(imageIndex);
        const VkImageLayout oldLayout = m_Swapchain->GetTrackedLayout(imageIndex);

        VkImageSubresourceRange range{};
        range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        range.baseMipLevel = 0;
        range.levelCount = 1;
        range.baseArrayLayer = 0;
        range.layerCount = 1;

        VkImageMemoryBarrier toTransfer{};
        toTransfer.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        toTransfer.srcAccessMask = 0;
        toTransfer.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        toTransfer.oldLayout = oldLayout;
        toTransfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        toTransfer.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toTransfer.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toTransfer.image = image;
        toTransfer.subresourceRange = range;

        vkCmdPipelineBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            0,
            0, nullptr,
            0, nullptr,
            1, &toTransfer
        );

        VkClearColorValue clearColor = { { 0.05f, 0.07f, 0.12f, 1.0f } };

        vkCmdClearColorImage(
            commandBuffer,
            image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            &clearColor,
            1,
            &range
        );

        VkImageMemoryBarrier toPresent{};
        toPresent.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        toPresent.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        toPresent.dstAccessMask = 0;
        toPresent.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        toPresent.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        toPresent.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toPresent.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toPresent.image = image;
        toPresent.subresourceRange = range;

        vkCmdPipelineBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
            0,
            0, nullptr,
            0, nullptr,
            1, &toPresent
        );

        if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS)
            throw std::runtime_error("Failed to end graphics command buffer.");

        m_Swapchain->SetTrackedLayout(imageIndex, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
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

        m_FrameSync.reset();
        m_GraphicsCommands.reset();
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

        m_TrianglePipeline = std::make_unique<Quiet::Renderer::Vulkan::TrianglePipeline>(
            *m_LogicalDevice,
            *m_Swapchain,
            *m_DepthResources
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

        oldSwapchain.reset();
        m_Window->ResetFramebufferResizedFlag();
        QUIET_CORE_INFO("Recreated swapchain-dependent resources.");
    }

    void Application::RecordTriangleCommandBuffer(std::uint32_t imageIndex)
    {
        const VkCommandBuffer commandBuffer = m_GraphicsCommands->GetCommandBuffer(imageIndex);

        vkResetCommandBuffer(commandBuffer, 0);

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS)
            throw std::runtime_error("Failed to begin graphics command buffer.");

        const VkImage colorImage = m_Swapchain->GetImages().at(imageIndex);
        const VkImageLayout oldColorLayout = m_Swapchain->GetTrackedLayout(imageIndex);

        VkImageSubresourceRange colorRange{};
        colorRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        colorRange.baseMipLevel = 0;
        colorRange.levelCount = 1;
        colorRange.baseArrayLayer = 0;
        colorRange.layerCount = 1;

        VkImageMemoryBarrier colorToAttachment{};
        colorToAttachment.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        colorToAttachment.oldLayout = oldColorLayout;
        colorToAttachment.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        colorToAttachment.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        colorToAttachment.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        colorToAttachment.image = colorImage;
        colorToAttachment.subresourceRange = colorRange;
        colorToAttachment.srcAccessMask = 0;
        colorToAttachment.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        vkCmdPipelineBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            0,
            0, nullptr,
            0, nullptr,
            1, &colorToAttachment
        );

        if (m_DepthResources->GetTrackedLayout() != VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
        {
            VkImageSubresourceRange depthRange{};
            depthRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
            depthRange.baseMipLevel = 0;
            depthRange.levelCount = 1;
            depthRange.baseArrayLayer = 0;
            depthRange.layerCount = 1;

            VkImageMemoryBarrier depthToAttachment{};
            depthToAttachment.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            depthToAttachment.oldLayout = m_DepthResources->GetTrackedLayout();
            depthToAttachment.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            depthToAttachment.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            depthToAttachment.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            depthToAttachment.image = m_DepthResources->GetImage();
            depthToAttachment.subresourceRange = depthRange;
            depthToAttachment.srcAccessMask = 0;
            depthToAttachment.dstAccessMask =
                VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

            vkCmdPipelineBarrier(
                commandBuffer,
                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
                0,
                0, nullptr,
                0, nullptr,
                1, &depthToAttachment
            );

            m_DepthResources->SetTrackedLayout(VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
        }

        VkClearValue colorClear{};
        colorClear.color = { { 0.05f, 0.07f, 0.12f, 1.0f } };

        VkClearValue depthClear{};
        depthClear.depthStencil = { 1.0f, 0 };

        VkRenderingAttachmentInfo colorAttachment{};
        colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        colorAttachment.imageView = m_Swapchain->GetImageViews().at(imageIndex);
        colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.clearValue = colorClear;

        VkRenderingAttachmentInfo depthAttachment{};
        depthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        depthAttachment.imageView = m_DepthResources->GetImageView();
        depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.clearValue = depthClear;

        VkRenderingInfo renderingInfo{};
        renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
        renderingInfo.renderArea.offset = { 0, 0 };
        renderingInfo.renderArea.extent = m_Swapchain->GetExtent();
        renderingInfo.layerCount = 1;
        renderingInfo.colorAttachmentCount = 1;
        renderingInfo.pColorAttachments = &colorAttachment;
        renderingInfo.pDepthAttachment = &depthAttachment;

        vkCmdBeginRendering(commandBuffer, &renderingInfo);

        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_TrianglePipeline->GetHandle());

        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = static_cast<float>(m_Swapchain->GetExtent().width);
        viewport.height = static_cast<float>(m_Swapchain->GetExtent().height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

        VkRect2D scissor{};
        scissor.offset = { 0, 0 };
        scissor.extent = m_Swapchain->GetExtent();
        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

        const VkBuffer vertexBuffers[] = { m_TriangleMesh->GetVertexBuffer() };
        const VkDeviceSize offsets[] = { 0 };

        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_TrianglePipeline->GetHandle());
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
        vkCmdDraw(commandBuffer, m_TriangleMesh->GetVertexCount(), 1, 0, 0);

        vkCmdEndRendering(commandBuffer);

        VkImageMemoryBarrier toPresent{};
        toPresent.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        toPresent.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        toPresent.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        toPresent.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toPresent.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toPresent.image = colorImage;
        toPresent.subresourceRange = colorRange;
        toPresent.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        toPresent.dstAccessMask = 0;

        vkCmdPipelineBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
            0,
            0, nullptr,
            0, nullptr,
            1, &toPresent
        );

        if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS)
            throw std::runtime_error("Failed to end graphics command buffer.");

        m_Swapchain->SetTrackedLayout(imageIndex, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
    }
}

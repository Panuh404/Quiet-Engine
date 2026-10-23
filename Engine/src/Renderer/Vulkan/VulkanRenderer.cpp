#include "VulkanRenderer.h"

#include "Quiet/Core/AppSpec.h"
#include "Quiet/Core/Log.h"
#include "Quiet/Platform/Window.h"
#include "Quiet/Assets/TexturePixelData.h"

#include <cmath>
#include <filesystem>
#include <stdexcept>
#include "GLFW/glfw3.h"
#include "Quiet/Assets/FontImporter.h"
#include "Quiet/Assets/TextureImporter.h"
#include "Quiet/Renderer/TextLayout.h"
#include "Quiet/UI/PaintCommands.h"

#include "Renderer/UI/ImGuiLayer.h"

#include "Renderer/Vulkan/RenderTypes.h"

#include "Renderer/Vulkan/Context/GraphicsCommands.h"
#include "Renderer/Vulkan/Context/LogicalDevice.h"
#include "Renderer/Vulkan/Context/PhysicalDevice.h"
#include "Renderer/Vulkan/Context/VulkanInstance.h"
#include "Renderer/Vulkan/Context/VulkanSurface.h"

#include "Renderer/Vulkan/Frame/CameraResources.h"
#include "Renderer/Vulkan/Frame/DepthResources.h"
#include "Renderer/Vulkan/Frame/FrameSync.h"
#include "Renderer/Vulkan/Frame/SceneViewportTarget.h"
#include "Renderer/Vulkan/Frame/Swapchain.h"

#include "Renderer/Vulkan/Mesh/MeshLibrary.h"
#include "Renderer/Vulkan/Mesh/StaticMeshMaterialResources.h"
#include "Renderer/Vulkan/Mesh/StaticMeshPipeline.h"
#include "Renderer/Vulkan/Mesh/StaticMeshRenderSystem.h"

#include "Renderer/Vulkan/Panel/PanelPipeline.h"

#include "Renderer/Vulkan/Text/TextPipeline.h"
#include "Renderer/Vulkan/Text/TextRenderSystem.h"
#include "Renderer/Vulkan/Texture/Texture2D.h"
#include "Renderer/Vulkan/Texture/TextureLibrary.h"

#include "Renderer/Vulkan/Editor/EditorGridPipeline.h"
#include "Renderer/Vulkan/Editor/EditorGridRenderSystem.h"
#include "Renderer/Vulkan/Editor/SelectionMaskPipeline.h"
#include "Renderer/Vulkan/Editor/SelectionMaskRenderSystem.h"
#include "Renderer/Vulkan/Editor/SelectionOutlinePipeline.h"
#include "Renderer/Vulkan/Editor/SelectionOutlineRenderSystem.h"

#include "Scene/Components/TextComponent.h"
#include "Scene/Components/UIRectComponent.h"
#include "Scene/Scene.h"

namespace
{
    #ifndef QUIET_ENGINE_RESOURCES_PATH
    #define QUIET_ENGINE_RESOURCES_PATH "Resources"
    #endif

    constexpr const char* kEditorTopbarLogoTextureID = "Editor.TopbarLogo";

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

    void SynchronizeDepthAttachmentForSelectionRead(VkCommandBuffer commandBuffer, VkImage depthImage)
     {
         VkImageMemoryBarrier barrier{};
         barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
         barrier.oldLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
         barrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
         barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
         barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
         barrier.image = depthImage;
         barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
         barrier.subresourceRange.baseMipLevel = 0;
         barrier.subresourceRange.levelCount = 1;
         barrier.subresourceRange.baseArrayLayer = 0;
         barrier.subresourceRange.layerCount = 1;
         barrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
         barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;

         const VkPipelineStageFlags depthStages =
             VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
             VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;

         vkCmdPipelineBarrier(
             commandBuffer,
             depthStages,
             depthStages,
             0,
             0,
             nullptr,
             0,
             nullptr,
             1,
             &barrier
         );
     }

    void SynchronizeColorAttachmentForOverlay(VkCommandBuffer commandBuffer, VkImage colorImage)
     {
         VkImageMemoryBarrier barrier{};
         barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
         barrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
         barrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
         barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
         barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
         barrier.image = colorImage;

         barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
         barrier.subresourceRange.baseMipLevel = 0;
         barrier.subresourceRange.levelCount = 1;
         barrier.subresourceRange.baseArrayLayer = 0;
         barrier.subresourceRange.layerCount = 1;

         barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
         barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

         vkCmdPipelineBarrier(
             commandBuffer,
             VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
             VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
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

namespace Quiet::Renderer::Vulkan
{
    VulkanRenderer::VulkanRenderer(const Core::AppSpec &spec, Platform::Window &window)
        : m_Window(&window), m_EditorMode(spec.IsEditor())
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

        m_ImGuiLayer = std::make_unique<UI::ImGuiLayer>(
            static_cast<GLFWwindow*>(window.GetNativeHandle()),
            m_VulkanInstance->GetHandle(),
            m_PhysicalDevice->GetHandle(),
            m_LogicalDevice->GetHandle(),
            m_PhysicalDevice->GetQueueFamilies().GraphicsFamily.value(),
            m_LogicalDevice->GetGraphicsQueue(),
            2,
            static_cast<std::uint32_t>(m_Swapchain->GetImages().size()),
            m_Swapchain->GetImageFormat(),
            m_DepthResources->GetFormat(),
            m_EditorMode ? "Quiet_Editor.ini" : nullptr,
            m_EditorMode,
            m_EditorMode);

        m_MeshLibrary = std::make_unique<MeshLibrary>();
        m_TextureLibrary = std::make_unique<TextureLibrary>();

        LoadEditorTextures();

        m_CameraResources = std::make_unique<CameraResources>(
            *m_PhysicalDevice,
            *m_LogicalDevice,
            *m_Swapchain);

        m_StaticMeshMaterialResources = std::make_unique<StaticMeshMaterialResources>(
            *m_PhysicalDevice,
            *m_LogicalDevice,
            *m_GraphicsCommands);

        m_StaticMeshPipeline = std::make_unique<StaticMeshPipeline>(
            *m_LogicalDevice,
            m_SceneViewportTarget->GetColorFormat(),
            m_SceneViewportTarget->GetDepthFormat(),
            m_CameraResources->GetDescriptorSetLayout(),
            m_StaticMeshMaterialResources->GetDescriptorSetLayout());

        if (m_EditorMode)
        {
            m_EditorGridPipeline = std::make_unique<EditorGridPipeline>(
                *m_LogicalDevice,
                m_SceneViewportTarget->GetColorFormat(),
                m_SceneViewportTarget->GetDepthFormat(),
                m_CameraResources->GetDescriptorSetLayout());

            m_EditorGridRenderSystem = std::make_unique<EditorGridRenderSystem>();

            m_SelectionMaskPipeline = std::make_unique<SelectionMaskPipeline>(
                *m_LogicalDevice,
                m_SceneViewportTarget->GetSelectionMaskFormat(),
                m_SceneViewportTarget->GetDepthFormat(),
                m_CameraResources->GetDescriptorSetLayout());

            m_SelectionMaskRenderSystem = std::make_unique<SelectionMaskRenderSystem>();
            m_SelectionOutlinePipeline = std::make_unique<SelectionOutlinePipeline>(*m_LogicalDevice, m_SceneViewportTarget->GetColorFormat());
            m_SelectionOutlineRenderSystem = std::make_unique<SelectionOutlineRenderSystem>();
        }

        m_FrameSync = std::make_unique<FrameSync>(*m_LogicalDevice, *m_Swapchain);
        m_StaticMeshRenderSystem = std::make_unique<StaticMeshRenderSystem>();

        const std::filesystem::path defaultFontPath = std::filesystem::path(QUIET_ENGINE_RESOURCES_PATH) / "Fonts" / "Inter-Regular.ttf";

        m_DefaultFontAtlas = Assets::FontImporter::LoadFromFile(defaultFontPath, 32.0f);
        if (!m_DefaultFontAtlas.Succeeded())
        {
            throw std::runtime_error("Failed to import default runtime font '" + defaultFontPath.string() + "': " + m_DefaultFontAtlas.Error);
        }

        m_DefaultFontTexture = std::make_unique<Texture2D>(*m_PhysicalDevice, *m_LogicalDevice, *m_GraphicsCommands, m_DefaultFontAtlas);
        m_TextPipeline = std::make_unique<TextPipeline>(*m_LogicalDevice, m_SceneViewportTarget->GetColorFormat(), *m_DefaultFontTexture);
        m_TextRenderSystem = std::make_unique<TextRenderSystem>();

        m_PanelPipeline = std::make_unique<PanelPipeline>(*m_LogicalDevice, m_SceneViewportTarget->GetColorFormat());

        QUIET_CORE_INFO("Loaded default runtime font '{}'. pixelHeight={} atlas={}x{}",
            defaultFontPath.string(),
            m_DefaultFontAtlas.PixelHeight,
            m_DefaultFontAtlas.Width,
            m_DefaultFontAtlas.Height);
    }

    void VulkanRenderer::DestroyRenderResources()
    {
        if (m_LogicalDevice)
        {
            vkDeviceWaitIdle(m_LogicalDevice->GetHandle());
        }

        m_FrameSync.reset();
        m_StaticMeshRenderSystem.reset();

        m_SelectionOutlineRenderSystem.reset();
        m_SelectionOutlinePipeline.reset();
        m_SelectionMaskRenderSystem.reset();
        m_SelectionMaskPipeline.reset();

        m_EditorGridRenderSystem.reset();
        m_EditorGridPipeline.reset();

        m_GraphicsCommands.reset();
        m_StaticMeshPipeline.reset();
        m_StaticMeshMaterialResources.reset();
        m_CameraResources.reset();
        m_SceneViewportTarget.reset();
        m_ImGuiLayer.reset();
        m_DepthResources.reset();
        m_MeshLibrary.reset();

        m_TextureLibrary.reset();
        m_PanelPipeline.reset();
        m_TextRenderSystem.reset();
        m_TextPipeline.reset();
        m_DefaultFontTexture.reset();

        m_Swapchain.reset();
        m_LogicalDevice.reset();
        m_PhysicalDevice.reset();
        m_VulkanSurface.reset();
        m_VulkanInstance.reset();
    }

    void VulkanRenderer::LoadEditorTextures()
    {
        if (!m_TextureLibrary ||  !m_PhysicalDevice || !m_LogicalDevice || !m_GraphicsCommands) return;

        const std::filesystem::path logoPath =
            std::filesystem::path(QUIET_ENGINE_RESOURCES_PATH) /
            "Textures" /
            "Editor" /
            "quiet-topbar.png";

        if (!std::filesystem::exists(logoPath))
        {
            QUIET_CORE_WARN("Editor topbar logo texture was not found at '{}'.", logoPath.string());
            return;
        }

        const Assets::TexturePixelData pixelData = Assets::TextureImporter::LoadPixelsFromFile(logoPath);
        if (!pixelData.Metadata.Succeeded() || !pixelData.IsValid())
        {
            QUIET_CORE_WARN(
                "Failed to load editor topbar logo texture from '{}': {}",
                logoPath.string(),
                pixelData.Metadata.Error
            );
            return;
        }

        TextureStats stats{};
        stats.Format = pixelData.Metadata.Format;
        stats.Width = pixelData.Width;
        stats.Height = pixelData.Height;
        stats.DimensionsValid = true;
        stats.FileSizeBytes = pixelData.Metadata.FileSizeBytes;
        stats.PixelBytes = pixelData.GetByteSize();
        stats.GpuReady = true;
        stats.Warnings = pixelData.Metadata.Warnings;
        stats.WarningCount = static_cast<std::uint32_t>(stats.Warnings.size());

        try
        {
            m_TextureLibrary->CreateTexture2D(
                kEditorTopbarLogoTextureID,
                *m_PhysicalDevice,
                *m_LogicalDevice,
                *m_GraphicsCommands,
                pixelData,
                stats
            );
        }
        catch (const std::exception& e)
        {
            QUIET_CORE_WARN(
                "Failed to create editor topbar logo GPU texture '{}': {}",
                kEditorTopbarLogoTextureID,
                e.what()
            );
        }
    }

    bool VulkanRenderer::DrawFrame(Scene::Scene& scene,
                                   const CameraUniformData& cameraData,
                                   UI::ImGuiLayer& imguiLayer,
                                   const bool drawEditorGrid,
                                   std::optional<Core::UUID> selectedActorID)
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

        m_CameraResources->Update(imageIndex, cameraData);
        RecordFrameCommandBuffer(imageIndex, scene, imguiLayer, drawEditorGrid, selectedActorID);

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

    void VulkanRenderer::WaitIdle() const
    {
        if (m_LogicalDevice)
            vkDeviceWaitIdle(m_LogicalDevice->GetHandle());
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

        if (m_ImGuiLayer) m_ImGuiLayer->ClearSceneViewportTexture();
        m_PanelPipeline.reset();
        m_TextPipeline.reset();

        m_SelectionOutlineRenderSystem.reset();
        m_SelectionOutlinePipeline.reset();
        m_SelectionMaskRenderSystem.reset();
        m_SelectionMaskPipeline.reset();
        m_EditorGridRenderSystem.reset();
        m_EditorGridPipeline.reset();
        m_SceneViewportTarget.reset();
        m_ImGuiLayer.reset();
        m_FrameSync.reset();
        m_GraphicsCommands.reset();
        m_StaticMeshPipeline.reset();
        m_StaticMeshMaterialResources.reset();
        m_CameraResources.reset();
        m_DepthResources.reset();

        auto oldSwapchain = std::move(m_Swapchain);

        m_Swapchain = std::make_unique<Swapchain>(
            *m_Window,
            *m_PhysicalDevice,
            *m_LogicalDevice,
            *m_VulkanSurface,
            oldSwapchain ? oldSwapchain->GetHandle() : VK_NULL_HANDLE);

        m_DepthResources = std::make_unique<DepthResources>(
            *m_PhysicalDevice,
            *m_LogicalDevice,
            *m_Swapchain);

        m_GraphicsCommands = std::make_unique<GraphicsCommands>(
            *m_PhysicalDevice,
            *m_LogicalDevice,
            *m_Swapchain);

        m_CameraResources = std::make_unique<CameraResources>(
            *m_PhysicalDevice,
            *m_LogicalDevice,
            *m_Swapchain);

        m_StaticMeshMaterialResources = std::make_unique<StaticMeshMaterialResources>(
            *m_PhysicalDevice,
            *m_LogicalDevice,
            *m_GraphicsCommands);

        m_SceneViewportTarget = std::make_unique<SceneViewportTarget>(
            m_PhysicalDevice->GetHandle(),
            m_LogicalDevice->GetHandle(),
            m_Swapchain->GetImageFormat(),
            m_DepthResources->GetFormat());

        m_StaticMeshPipeline = std::make_unique<StaticMeshPipeline>(
            *m_LogicalDevice,
            m_SceneViewportTarget->GetColorFormat(),
            m_SceneViewportTarget->GetDepthFormat(),
            m_CameraResources->GetDescriptorSetLayout(),
            m_StaticMeshMaterialResources->GetDescriptorSetLayout());

        m_TextPipeline = std::make_unique<TextPipeline>(
            *m_LogicalDevice,
            m_SceneViewportTarget->GetColorFormat(),
            *m_DefaultFontTexture);

        m_PanelPipeline = std::make_unique<PanelPipeline>(
            *m_LogicalDevice,
            m_SceneViewportTarget->GetColorFormat());

        if (m_EditorMode)
        {
            m_EditorGridPipeline = std::make_unique<EditorGridPipeline>(
                *m_LogicalDevice,
                m_SceneViewportTarget->GetColorFormat(),
                m_SceneViewportTarget->GetDepthFormat(),
                m_CameraResources->GetDescriptorSetLayout());

            m_EditorGridRenderSystem = std::make_unique<EditorGridRenderSystem>();

            m_SelectionMaskPipeline = std::make_unique<SelectionMaskPipeline>(
                *m_LogicalDevice,
                m_SceneViewportTarget->GetSelectionMaskFormat(),
                m_SceneViewportTarget->GetDepthFormat(),
                m_CameraResources->GetDescriptorSetLayout()
            );

            m_SelectionMaskRenderSystem = std::make_unique<SelectionMaskRenderSystem>();

            m_SelectionOutlinePipeline = std::make_unique<SelectionOutlinePipeline>(
                *m_LogicalDevice,
                m_SceneViewportTarget->GetColorFormat());

            m_SelectionOutlineRenderSystem = std::make_unique<SelectionOutlineRenderSystem>();
        }

        m_FrameSync = std::make_unique<FrameSync>(
            *m_LogicalDevice,
            *m_Swapchain
        );

        m_ImGuiLayer = std::make_unique<UI::ImGuiLayer>(
        static_cast<GLFWwindow*>(m_Window->GetNativeHandle()),
            m_VulkanInstance->GetHandle(),
            m_PhysicalDevice->GetHandle(),
            m_LogicalDevice->GetHandle(),
            m_PhysicalDevice->GetQueueFamilies().GraphicsFamily.value(),
            m_LogicalDevice->GetGraphicsQueue(),
            2,
            static_cast<std::uint32_t>(m_Swapchain->GetImages().size()),
            m_Swapchain->GetImageFormat(),
            m_DepthResources->GetFormat(),
            m_EditorMode ? "Quiet_Editor.ini" : nullptr,
            m_EditorMode,
            m_EditorMode
        );

        m_SceneViewportReady = false;
        m_ViewportResizePending = false;
        m_PendingViewportWidth = 0;
        m_PendingViewportHeight = 0;
        m_ViewportResizeStableFrames = 0;

        oldSwapchain.reset();
        m_Window->ResetFramebufferResizedFlag();
        QUIET_CORE_INFO("Recreated swapchain-dependent resources.");
    }

    void VulkanRenderer::ApplyPendingViewportResize()
    {
        EnsureSceneViewportSize();
    }

    ImTextureID VulkanRenderer::GetSceneViewportTextureID()
    {
        if (!m_ImGuiLayer ||
        !m_SceneViewportTarget ||
        !m_SceneViewportTarget->IsValid() ||
        !m_SceneViewportReady)
            return ImTextureID{};

        return m_ImGuiLayer->GetOrCreateSceneViewportTextureID(
            m_SceneViewportTarget->GetSampler(),
            m_SceneViewportTarget->GetColorImageView(),
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        );
    }

    VkExtent2D VulkanRenderer::GetSwapchainExtent() const
    {
        return m_Swapchain->GetExtent();
    }

    VkExtent2D VulkanRenderer::GetSceneViewportExtent() const
    {
        if (m_SceneViewportTarget && m_SceneViewportTarget->IsValid())
        {
            return m_SceneViewportTarget->GetExtent();
        }
        return { 0, 0 };
    }

    void VulkanRenderer::RequestViewportResize(std::uint32_t width, std::uint32_t height)
    {
        if (m_PendingViewportWidth != width || m_PendingViewportHeight != height)
        {
            m_PendingViewportWidth = width;
            m_PendingViewportHeight = height;
            m_ViewportResizeStableFrames = 0;
        }

        m_ViewportResizePending = true;
    }

    void VulkanRenderer::EnsureSceneViewportSize()
    {
        if (!m_ViewportResizePending || m_PendingViewportWidth == 0 || m_PendingViewportHeight == 0)
            return;

        const VkExtent2D currentExtent = GetSceneViewportExtent();
        if (currentExtent.width == m_PendingViewportWidth &&
            currentExtent.height == m_PendingViewportHeight)
        {
            m_ViewportResizePending = false;
            m_ViewportResizeStableFrames = 0;
            return;
        }

        ++m_ViewportResizeStableFrames;
        if (m_ViewportResizeStableFrames < 3) return;

        vkDeviceWaitIdle(m_LogicalDevice->GetHandle());
        m_ImGuiLayer->ClearSceneViewportTexture();

        m_SceneViewportTarget->Resize(
            m_PendingViewportWidth,
            m_PendingViewportHeight
        );

        if (m_EditorMode && m_SelectionOutlinePipeline && m_SceneViewportTarget->IsValid())
        {
            m_SelectionOutlinePipeline->UpdateMaskImage(
                m_SceneViewportTarget->GetSampler(),
                m_SceneViewportTarget->GetSelectionMaskImageView()
            );
        }

        m_ViewportResizePending = false;
        m_ViewportResizeStableFrames = 0;
        m_SceneViewportReady = false;
    }

    void VulkanRenderer::RecordFrameCommandBuffer(std::uint32_t imageIndex,
                                                  Scene::Scene& scene,
                                                  UI::ImGuiLayer& imguiLayer,
                                                  const bool drawEditorGrid,
                                                  const std::optional<Core::UUID>& selectedActorID)
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
            renderContext.Materials = m_StaticMeshMaterialResources.get();
            renderContext.Camera = m_CameraResources.get();
            renderContext.Textures = m_TextureLibrary.get();
            renderContext.SceneData = &scene;
            renderContext.Extent = m_SceneViewportTarget->GetExtent();

            m_StaticMeshRenderSystem->Record(renderContext);

            if (drawEditorGrid && m_EditorGridPipeline && m_EditorGridRenderSystem)
            {
                EditorGridRenderContext gridContext{};
                gridContext.CommandBuffer = commandBuffer;
                gridContext.ImageIndex = imageIndex;
                gridContext.Pipeline = m_EditorGridPipeline.get();
                gridContext.Camera = m_CameraResources.get();
                gridContext.Extent = m_SceneViewportTarget->GetExtent();

                m_EditorGridRenderSystem->Record(gridContext);
            }

            vkCmdEndRendering(commandBuffer);

            // Selected Entity Pass
            const entt::registry& registry = scene.GetRegistry();
            entt::entity selectedEntity = entt::null;

            if (selectedActorID.has_value())
            {
                selectedEntity = scene.FindEntityByID(selectedActorID.value());
            }

            if (m_EditorMode && m_SelectionMaskPipeline && m_SelectionMaskRenderSystem)
            {
                SynchronizeDepthAttachmentForSelectionRead(commandBuffer, m_SceneViewportTarget->GetDepthImage());

                TransitionImageLayout(
                    commandBuffer,
                    m_SceneViewportTarget->GetSelectionMaskImage(),
                    VK_IMAGE_ASPECT_COLOR_BIT,
                    m_SceneViewportTarget->GetSelectionMaskLayout(),
                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
                );

                m_SceneViewportTarget->SetSelectionMaskLayout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

                VkClearValue selectionMaskClear{};
                selectionMaskClear.color.float32[0] = 0.0f;

                VkRenderingAttachmentInfo maskAttachment{};
                maskAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
                maskAttachment.imageView = m_SceneViewportTarget->GetSelectionMaskImageView();
                maskAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                maskAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
                maskAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
                maskAttachment.clearValue = selectionMaskClear;

                VkRenderingAttachmentInfo maskDepthAttachment{};
                maskDepthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
                maskDepthAttachment.imageView = m_SceneViewportTarget->GetDepthImageView();
                maskDepthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
                maskDepthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
                maskDepthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

                VkRenderingInfo maskRenderingInfo{};
                maskRenderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
                maskRenderingInfo.renderArea.offset = { 0, 0 };
                maskRenderingInfo.renderArea.extent = m_SceneViewportTarget->GetExtent();
                maskRenderingInfo.layerCount = 1;
                maskRenderingInfo.colorAttachmentCount = 1;
                maskRenderingInfo.pColorAttachments = &maskAttachment;
                maskRenderingInfo.pDepthAttachment = &maskDepthAttachment;
                maskRenderingInfo.pStencilAttachment = nullptr;

                vkCmdBeginRendering(commandBuffer, &maskRenderingInfo);

                SelectionMaskRenderContext maskContext{};
                maskContext.CommandBuffer = commandBuffer;
                maskContext.ImageIndex = imageIndex;
                maskContext.Pipeline = m_SelectionMaskPipeline.get();
                maskContext.Camera = m_CameraResources.get();
                maskContext.Registry = &registry;
                maskContext.SelectedEntity = selectedEntity;
                maskContext.Extent = m_SceneViewportTarget->GetExtent();

                m_SelectionMaskRenderSystem->Record(maskContext);

                vkCmdEndRendering(commandBuffer);

                TransitionImageLayout(
                    commandBuffer,
                    m_SceneViewportTarget->GetSelectionMaskImage(),
                    VK_IMAGE_ASPECT_COLOR_BIT,
                    m_SceneViewportTarget->GetSelectionMaskLayout(),
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                );

                m_SceneViewportTarget->SetSelectionMaskLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

                const Scene::StaticMeshComponent* selectedStaticMesh = selectedEntity != entt::null &&
                    registry.valid(selectedEntity) ? registry.try_get<Scene::StaticMeshComponent>(selectedEntity) : nullptr;

                const bool selectedStaticMeshVisible =
                    selectedStaticMesh &&
                    selectedStaticMesh->IsValid() &&
                    selectedStaticMesh->Visible;

                if (selectedStaticMeshVisible &&
                    m_EditorMode &&
                    m_SelectionOutlinePipeline &&
                    m_SelectionOutlineRenderSystem &&
                    m_SelectionOutlinePipeline->IsMaskImageReady())
                {
                    SynchronizeColorAttachmentForOverlay(commandBuffer, m_SceneViewportTarget->GetColorImage());

                    VkRenderingAttachmentInfo outlineAttachment{};
                    outlineAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
                    outlineAttachment.imageView = m_SceneViewportTarget->GetColorImageView();
                    outlineAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                    outlineAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
                    outlineAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

                    VkRenderingInfo outlineRenderingInfo{};
                    outlineRenderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
                    outlineRenderingInfo.renderArea.offset = { 0, 0 };
                    outlineRenderingInfo.renderArea.extent = m_SceneViewportTarget->GetExtent();
                    outlineRenderingInfo.layerCount = 1;
                    outlineRenderingInfo.colorAttachmentCount = 1;
                    outlineRenderingInfo.pColorAttachments = &outlineAttachment;
                    outlineRenderingInfo.pDepthAttachment = nullptr;
                    outlineRenderingInfo.pStencilAttachment = nullptr;

                    vkCmdBeginRendering(commandBuffer, &outlineRenderingInfo);

                    SelectionOutlineRenderContext outlineContext{};
                    outlineContext.CommandBuffer = commandBuffer;
                    outlineContext.Pipeline = m_SelectionOutlinePipeline.get();
                    outlineContext.Extent = m_SceneViewportTarget->GetExtent();

                    m_SelectionOutlineRenderSystem->Record(outlineContext);

                    vkCmdEndRendering(commandBuffer);
                }
            }

            // Ordered paint preview, independent of Scene components.
            // Producers remain local until retained widgets are introduced.
            if (m_PanelPipeline || (m_TextPipeline && m_TextRenderSystem))
            {
                const VkExtent2D paintExtent = m_SceneViewportTarget->GetExtent();
                if (paintExtent.width > 0 && paintExtent.height > 0)
                {
                    Quiet::UI::PaintList paintList;
                    paintList.reserve(2);
                    paintList.emplace_back(Quiet::UI::SolidRectCommand
                    {
                        glm::vec2(24.0f, 24.0f),
                        glm::vec2(344.0f, 144.0f),
                        glm::vec4(0.03f, 0.12f, 0.25f, 0.8f)
                    });
                    paintList.emplace_back(Quiet::UI::TextCommand
                    {
                        "Paint order preview",
                        glm::vec2(40.0f, 48.0f),
                        32.0f,
                        glm::vec4(1.0f)
                    });

                    SynchronizeColorAttachmentForOverlay(commandBuffer, m_SceneViewportTarget->GetColorImage());

                    VkRenderingAttachmentInfo paintAttachment{};
                    paintAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
                    paintAttachment.imageView = m_SceneViewportTarget->GetColorImageView();
                    paintAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                    paintAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
                    paintAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

                    VkRenderingInfo paintRenderingInfo{};
                    paintRenderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
                    paintRenderingInfo.renderArea.extent = paintExtent;
                    paintRenderingInfo.layerCount = 1;
                    paintRenderingInfo.colorAttachmentCount = 1;
                    paintRenderingInfo.pColorAttachments = &paintAttachment;

                    vkCmdBeginRendering(commandBuffer, &paintRenderingInfo);

                    VkViewport paintViewport{};
                    paintViewport.width = static_cast<float>(paintExtent.width);
                    paintViewport.height = static_cast<float>(paintExtent.height);
                    paintViewport.minDepth = 0.0f;
                    paintViewport.maxDepth = 1.0f;

                    vkCmdSetViewport(commandBuffer, 0, 1, &paintViewport);

                    VkRect2D paintScissor{};
                    paintScissor.extent = paintExtent;

                    vkCmdSetScissor(commandBuffer, 0, 1, &paintScissor);

                    for (const Quiet::UI::PaintCommand& command : paintList)
                    {
                        if (const auto* rectangle = std::get_if<Quiet::UI::SolidRectCommand>(&command))
                        {
                            if (!m_PanelPipeline) continue;

                            vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_PanelPipeline->GetHandle());

                            PanelPushConstants push{};
                            push.Rect = glm::vec4(rectangle->Min.x, rectangle->Min.y, rectangle->Max.x, rectangle->Max.y);
                            push.Color = rectangle->Color;
                            push.Viewport = glm::vec4(
                                static_cast<float>(paintExtent.width),
                                static_cast<float>(paintExtent.height),
                                0.0f,
                                0.0f);

                            vkCmdPushConstants(
                                commandBuffer,
                                m_PanelPipeline->GetLayout(),
                                VK_SHADER_STAGE_VERTEX_BIT,
                                0,
                                sizeof(PanelPushConstants),
                                &push);

                            vkCmdDraw(commandBuffer, 6, 1, 0, 0);
                        }
                        else if (const auto* text = std::get_if<Quiet::UI::TextCommand>(&command))
                        {
                            if (!m_TextPipeline || !m_TextRenderSystem) continue;

                            const TextLayoutResult layout = TextLayout::BuildUTF8(m_DefaultFontAtlas, text->Content, text->PixelHeight);
                            if (!layout.Succeeded() || layout.Quads.empty()) continue;

                            TextRenderContext context{};
                            context.CommandBuffer = commandBuffer;
                            context.Pipeline = m_TextPipeline.get();
                            context.Extent = paintExtent;
                            context.Quads = std::span(layout.Quads);
                            context.Offset = text->Origin;
                            context.Color = text->Color;

                            m_TextRenderSystem->Record(context);
                        }
                    }
                    vkCmdEndRendering(commandBuffer);
                }
            }

            // Existing Actor-authored text path, preserved while the
            // retained runtime UI system is introduced incrementally.
            if (m_TextPipeline && m_TextRenderSystem)
            {
                SynchronizeColorAttachmentForOverlay(commandBuffer, m_SceneViewportTarget->GetColorImage());

                VkRenderingAttachmentInfo textAttachment{};
                textAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
                textAttachment.imageView = m_SceneViewportTarget->GetColorImageView();
                textAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                textAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
                textAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

                VkRenderingInfo textRenderingInfo{};
                textRenderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
                textRenderingInfo.renderArea.extent = m_SceneViewportTarget->GetExtent();
                textRenderingInfo.layerCount = 1;
                textRenderingInfo.colorAttachmentCount = 1;
                textRenderingInfo.pColorAttachments = &textAttachment;

                vkCmdBeginRendering(commandBuffer, &textRenderingInfo);

                for (const Scene::Actor& actor : scene.GetActors())
                {
                    const auto* text = actor.TryGetComponent<Scene::TextComponent>();
                    if (!text || !text->Visible || text->Content.empty()) continue;

                    Quiet::UI::TextCommand textCommand
                    {
                        text->Content,
                        glm::vec2(24.0f, 24.0f),
                        text->PixelHeight,
                        text->Color
                    };

                    const TextLayoutResult layout = TextLayout::BuildUTF8(m_DefaultFontAtlas, textCommand.Content, textCommand.PixelHeight);
                    if (!layout.Succeeded() || layout.Quads.empty()) continue;

                    const VkExtent2D targetExtent = m_SceneViewportTarget->GetExtent();
                    glm::vec2 origin = textCommand.Origin;

                    if (const auto* uiRect = actor.TryGetComponent<Scene::UIRectComponent>())
                    {
                        if (!std::isfinite(uiRect->Size.x) || !std::isfinite(uiRect->Size.y) ||
                            uiRect->Size.x < 0.0f || uiRect->Size.y < 0.0f)
                            continue;

                        const glm::vec2 resolvedSize(
                            uiRect->Size.x > 0.0f ? uiRect->Size.x : layout.Size.x,
                            uiRect->Size.y > 0.0f ? uiRect->Size.y : layout.Size.y);

                        const glm::vec2 targetSize(
                            static_cast<float>(targetExtent.width),
                            static_cast<float>(targetExtent.height));

                        origin =
                            uiRect->Anchor * targetSize +
                            uiRect->Offset -
                            uiRect->Pivot * resolvedSize;
                    }

                    if (!std::isfinite(origin.x) || !std::isfinite(origin.y)) continue;
                    textCommand.Origin = origin;

                    TextRenderContext textContext{};
                    textContext.CommandBuffer = commandBuffer;
                    textContext.Pipeline = m_TextPipeline.get();
                    textContext.Extent = targetExtent;
                    textContext.Quads = std::span(layout.Quads);
                    textContext.Offset = textCommand.Origin;
                    textContext.Color = textCommand.Color;

                    m_TextRenderSystem->Record(textContext);
                }

                vkCmdEndRendering(commandBuffer);
            }

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

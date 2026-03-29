#pragma once

#include <cstdint>
#include <vector>
#include <vulkan/vulkan.h>

namespace Quiet::Renderer::Vulkan
{
    class LogicalDevice;
    class Swapchain;

    class FrameSync
    {
    public:
        FrameSync(const LogicalDevice& logicalDevice, const Swapchain& swapchain);
        ~FrameSync();

        FrameSync(const FrameSync&) = delete;
        FrameSync& operator=(const FrameSync&) = delete;
        FrameSync(FrameSync&&) = delete;
        FrameSync& operator=(FrameSync&&) = delete;

        [[nodiscard]] VkSemaphore GetImageAvailableSemaphore() const { return m_ImageAvailableSemaphore; }
        [[nodiscard]] VkSemaphore GetRenderFinishedSemaphore(std::uint32_t imageIndex) const { return m_RenderFinishedSemaphores.at(imageIndex); }
        [[nodiscard]] VkFence GetInFlightFence() const { return m_InFlightFence; }

    private:
        VkDevice m_Device = VK_NULL_HANDLE;
        VkSemaphore m_ImageAvailableSemaphore = VK_NULL_HANDLE;
        std::vector<VkSemaphore> m_RenderFinishedSemaphores;
        VkFence m_InFlightFence = VK_NULL_HANDLE;
    };
}
#pragma once

#include <vulkan/vulkan.h>

namespace Quiet::Renderer::Vulkan
{
    class PhysicalDevice;
    class LogicalDevice;
    class Swapchain;

    class DepthResources
    {
    public:
        DepthResources(const PhysicalDevice& physicalDevice,
                       const LogicalDevice& logicalDevice,
                       const Swapchain& swapchain);
        ~DepthResources();

        DepthResources(const DepthResources&) = delete;
        DepthResources& operator=(const DepthResources&) = delete;
        DepthResources(DepthResources&&) = delete;
        DepthResources& operator=(DepthResources&&) = delete;

        [[nodiscard]] VkImage GetImage() const { return m_Image; }
        [[nodiscard]] VkImageView GetImageView() const { return m_ImageView; }
        [[nodiscard]] VkFormat GetFormat() const { return m_Format; }
        [[nodiscard]] VkExtent2D GetExtent() const { return m_Extent; }
        [[nodiscard]] VkImageLayout GetTrackedLayout() const { return m_TrackedLayout; }

        void SetTrackedLayout(VkImageLayout layout) { m_TrackedLayout = layout; }

    private:
        VkDevice m_Device = VK_NULL_HANDLE;
        VkImage m_Image = VK_NULL_HANDLE;
        VkDeviceMemory m_Memory = VK_NULL_HANDLE;
        VkImageView m_ImageView = VK_NULL_HANDLE;
        VkFormat m_Format = VK_FORMAT_UNDEFINED;
        VkExtent2D m_Extent{};
        VkImageLayout m_TrackedLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    };
}
#pragma once
#include <cstdint>
#include <vector>

#include <vulkan/vulkan.h>

namespace Quiet::Platform
{
    class Window;
}

namespace Quiet::Renderer::Vulkan
{
    class PhysicalDevice;
    class LogicalDevice;
    class VulkanSurface;

    class Swapchain
    {
    public:
        Swapchain(const Quiet::Platform::Window& window,
                  const PhysicalDevice& physicalDevice,
                  const LogicalDevice& logicalDevice,
                  const VulkanSurface& surface);
        ~Swapchain();

        Swapchain(const Swapchain&) = delete;
        Swapchain& operator=(const Swapchain&) = delete;
        Swapchain(Swapchain&&) = delete;
        Swapchain& operator=(Swapchain&&) = delete;

        [[nodiscard]] VkSwapchainKHR GetHandle() const { return m_Swapchain; }
        [[nodiscard]] VkFormat GetImageFormat() const { return m_ImageFormat; }
        [[nodiscard]] VkExtent2D GetExtent() const { return m_Extent; }

        [[nodiscard]] const std::vector<VkImage>& GetImages() const { return m_Images; }
        [[nodiscard]] const std::vector<VkImageView>& GetImageViews() const { return m_ImageViews; }

        [[nodiscard]] VkImageLayout GetTrackedLayout(std::uint32_t imageIndex) const
        {
            return m_ImageLayouts.at(imageIndex);
        }

        void SetTrackedLayout(std::uint32_t imageIndex, VkImageLayout layout)
        {
            m_ImageLayouts.at(imageIndex) = layout;
        }

    private:
        VkDevice m_Device = VK_NULL_HANDLE;
        VkSwapchainKHR m_Swapchain = VK_NULL_HANDLE;
        VkFormat m_ImageFormat = VK_FORMAT_UNDEFINED;
        VkExtent2D m_Extent{};

        std::vector<VkImage> m_Images;
        std::vector<VkImageView> m_ImageViews;
        std::vector<VkImageLayout> m_ImageLayouts;
    };
}
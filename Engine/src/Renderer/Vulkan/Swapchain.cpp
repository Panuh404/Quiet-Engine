#include "Swapchain.h"

#include <algorithm>
#include <array>
#include <limits>
#include <stdexcept>

#include "LogicalDevice.h"
#include "PhysicalDevice.h"
#include "VulkanSurface.h"
#include "Quiet/Core/Log.h"
#include "Quiet/Platform/Window.h"

namespace
{
    VkSurfaceFormatKHR ChooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats)
    {
        for (const auto& format : availableFormats)
        {
            if (format.format == VK_FORMAT_B8G8R8A8_SRGB && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
                return format;
        }
        return availableFormats.front();
    }

    VkPresentModeKHR ChoosePresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes)
    {
        for (const auto presentMode : availablePresentModes)
        {
            if (presentMode == VK_PRESENT_MODE_MAILBOX_KHR) return presentMode;
        }
        return VK_PRESENT_MODE_FIFO_KHR;
    }

    VkExtent2D ChooseExtent(const Quiet::Platform::Window& window, const VkSurfaceCapabilitiesKHR& capabilities)
    {
        if (capabilities.currentExtent.width != std::numeric_limits<std::uint32_t>::max())
            return capabilities.currentExtent;

        const auto [fbWidth, fbHeight] = window.GetFramebufferSize();

        VkExtent2D actualExtent{};
        actualExtent.width = std::clamp(
            fbWidth,
            capabilities.minImageExtent.width,
            capabilities.maxImageExtent.width
        );
        actualExtent.height = std::clamp(
            fbHeight,
            capabilities.minImageExtent.height,
            capabilities.maxImageExtent.height
        );

        return actualExtent;
    }
}

namespace Quiet::Renderer::Vulkan
{
    Swapchain::Swapchain(const Quiet::Platform::Window &window,
                         const PhysicalDevice &physicalDevice,
                         const LogicalDevice &logicalDevice,
                         const VulkanSurface &surface)
            : m_Device(logicalDevice.GetHandle())
    {
        const auto& swapchainSupport = physicalDevice.GetSwapchainSupport();
        const auto& queueFamilies = physicalDevice.GetQueueFamilies();

        const VkSurfaceFormatKHR surfaceFormat = ChooseSurfaceFormat(swapchainSupport.Formats);
        const VkPresentModeKHR presentMode = ChoosePresentMode(swapchainSupport.PresentModes);
        const VkExtent2D extent = ChooseExtent(window, swapchainSupport.Capabilities);

        std::uint32_t imageCount = swapchainSupport.Capabilities.minImageCount + 1;
        if (swapchainSupport.Capabilities.maxImageCount > 0 && imageCount > swapchainSupport.Capabilities.maxImageCount)
        {
            imageCount = swapchainSupport.Capabilities.maxImageCount;
        }

        const std::array<std::uint32_t, 2> queueFamilyIndices = {
            queueFamilies.GraphicsFamily.value(),
            queueFamilies.PresentFamily.value()
        };

        VkSwapchainCreateInfoKHR createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        createInfo.surface = surface.GetHandle();
        createInfo.minImageCount = imageCount;
        createInfo.imageFormat = surfaceFormat.format;
        createInfo.imageColorSpace = surfaceFormat.colorSpace;
        createInfo.imageExtent = extent;
        createInfo.imageArrayLayers = 1;
        createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

        if (queueFamilies.GraphicsFamily != queueFamilies.PresentFamily)
        {
            createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            createInfo.queueFamilyIndexCount = static_cast<std::uint32_t>(queueFamilyIndices.size());
            createInfo.pQueueFamilyIndices = queueFamilyIndices.data();
        }
        else
        {
            createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
            createInfo.queueFamilyIndexCount = 0;
            createInfo.pQueueFamilyIndices = nullptr;
        }

        createInfo.preTransform = swapchainSupport.Capabilities.currentTransform;
        createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        createInfo.presentMode = presentMode;
        createInfo.clipped = VK_TRUE;
        createInfo.oldSwapchain = VK_NULL_HANDLE;

        if (vkCreateSwapchainKHR(m_Device, &createInfo, nullptr, &m_Swapchain) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create Vulkan swapchain.");
        }

        std::uint32_t swapchainImageCount = 0;
        if (vkGetSwapchainImagesKHR(m_Device, m_Swapchain, &swapchainImageCount, nullptr) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to query Vulkan swapchain image count.");
        }

        m_Images.resize(swapchainImageCount);
        if (vkGetSwapchainImagesKHR(m_Device, m_Swapchain, &swapchainImageCount, m_Images.data()) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to retrieve Vulkan swapchain images.");
        }

        m_ImageFormat = surfaceFormat.format;
        m_Extent = extent;

        QUIET_CORE_INFO(
            "Created swapchain. images={} format={} extent={}x{} presentMode={}",
            m_Images.size(),
            static_cast<int>(m_ImageFormat),
            m_Extent.width,
            m_Extent.height,
            static_cast<int>(presentMode)
        );
    }

    Swapchain::~Swapchain()
    {
        if (m_Swapchain != VK_NULL_HANDLE)
        {
            vkDestroySwapchainKHR(m_Device, m_Swapchain, nullptr);
            m_Swapchain = VK_NULL_HANDLE;
        }
    }
}

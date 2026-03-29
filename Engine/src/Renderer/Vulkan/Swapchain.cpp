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

    VkImageView CreateColorImageView(VkDevice device, VkImage image, VkFormat format)
    {
        VkImageViewCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        createInfo.image = image;
        createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        createInfo.format = format;

        createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

        createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        createInfo.subresourceRange.baseMipLevel = 0;
        createInfo.subresourceRange.levelCount = 1;
        createInfo.subresourceRange.baseArrayLayer = 0;
        createInfo.subresourceRange.layerCount = 1;

        VkImageView view = VK_NULL_HANDLE;
        if (vkCreateImageView(device, &createInfo, nullptr, &view) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create swapchain image view.");
        }

        return view;

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

        const auto supportedUsageFlags = swapchainSupport.Capabilities.supportedUsageFlags;
        if ((supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT) == 0)
        {
            throw std::runtime_error("Swapchain images do not support VK_IMAGE_USAGE_TRANSFER_DST_BIT on this surface.");
        }
        createInfo.imageUsage =
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
            VK_IMAGE_USAGE_TRANSFER_DST_BIT;

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

        m_ImageViews.reserve(m_Images.size());
        m_ImageLayouts.resize(m_Images.size(), VK_IMAGE_LAYOUT_UNDEFINED);
        
        try
        {
            for (VkImage image : m_Images)
            {
                m_ImageViews.push_back(CreateColorImageView(m_Device, image, m_ImageFormat));
            }
        }
        catch (...)
        {
            for (VkImageView view : m_ImageViews)
            {
                if (view != VK_NULL_HANDLE)
                    vkDestroyImageView(m_Device, view, nullptr);
            }
            m_ImageViews.clear();

            if (m_Swapchain != VK_NULL_HANDLE)
            {
                vkDestroySwapchainKHR(m_Device, m_Swapchain, nullptr);
                m_Swapchain = VK_NULL_HANDLE;
            }

            throw;
        }

        QUIET_CORE_INFO(
            "Created swapchain. images={} imageViews={} format={} extent={}x{} presentMode={}",
            m_Images.size(),
            m_ImageViews.size(),
            static_cast<int>(m_ImageFormat),
            m_Extent.width,
            m_Extent.height,
            static_cast<int>(presentMode)
        );
    }

    Swapchain::~Swapchain()
    {
        for (VkImageView view : m_ImageViews)
        {
            if (view != VK_NULL_HANDLE)
            {
                vkDestroyImageView(m_Device, view, nullptr);
            }
        }
        m_ImageViews.clear();

        if (m_Swapchain != VK_NULL_HANDLE)
        {
            vkDestroySwapchainKHR(m_Device, m_Swapchain, nullptr);
            m_Swapchain = VK_NULL_HANDLE;
        }
    }
}

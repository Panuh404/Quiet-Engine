#include "DepthResources.h"

#include <array>
#include <stdexcept>

#include "LogicalDevice.h"
#include "PhysicalDevice.h"
#include "Swapchain.h"
#include "Quiet/Core/Log.h"

namespace
{
    bool HasStencilComponent(VkFormat format)
    {
        return format == VK_FORMAT_D32_SFLOAT_S8_UINT ||
               format == VK_FORMAT_D24_UNORM_S8_UINT;
    }

    VkFormat FindDepthFormat(VkPhysicalDevice physicalDevice)
    {
        constexpr std::array<VkFormat, 3> candidates = {
            VK_FORMAT_D32_SFLOAT,
            VK_FORMAT_D32_SFLOAT_S8_UINT,
            VK_FORMAT_D24_UNORM_S8_UINT
        };

        for (VkFormat format : candidates)
        {
            VkFormatProperties props{};
            vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &props);

            if ((props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0)
                return format;
        }
        throw std::runtime_error("Failed to find a supported Vulkan depth format.");
    }
}

namespace Quiet::Renderer::Vulkan
{
    DepthResources::DepthResources(const PhysicalDevice &physicalDevice,
                                   const LogicalDevice &logicalDevice,
                                   const Swapchain &swapchain)
        : m_Device(logicalDevice.GetHandle())
    {
        m_Extent = swapchain.GetExtent();
        m_Format = FindDepthFormat(physicalDevice.GetHandle());

        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent.width = m_Extent.width;
        imageInfo.extent.height = m_Extent.height;
        imageInfo.extent.depth = 1;
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.format = m_Format;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        if (vkCreateImage(m_Device, &imageInfo, nullptr, &m_Image) != VK_SUCCESS)
            throw std::runtime_error("Failed to create Vulkan depth image.");

        VkMemoryRequirements memRequirements;
        vkGetImageMemoryRequirements(m_Device, m_Image, &memRequirements);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = physicalDevice.FindMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        if (vkAllocateMemory(m_Device, &allocInfo, nullptr, &m_Memory) != VK_SUCCESS)
        {
            vkDestroyImage(m_Device, m_Image, nullptr);
            m_Image = VK_NULL_HANDLE;
            throw std::runtime_error("Failed to allocate Vulkan depth image memory.");
        }

        if (vkBindImageMemory(m_Device, m_Image, m_Memory, 0) != VK_SUCCESS)
        {
            vkFreeMemory(m_Device, m_Memory, nullptr);
            vkDestroyImage(m_Device, m_Image, nullptr);
            m_Memory = VK_NULL_HANDLE;
            m_Image = VK_NULL_HANDLE;
            throw std::runtime_error("Failed to bind Vulkan depth image memory.");
        }

        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = m_Image;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = m_Format;
        viewInfo.subresourceRange.aspectMask =
            HasStencilComponent(m_Format)
                ? (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT)
                : VK_IMAGE_ASPECT_DEPTH_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        if (vkCreateImageView(m_Device, &viewInfo, nullptr, &m_ImageView) != VK_SUCCESS)
        {
            vkFreeMemory(m_Device, m_Memory, nullptr);
            vkDestroyImage(m_Device, m_Image, nullptr);
            m_Memory = VK_NULL_HANDLE;
            m_Image = VK_NULL_HANDLE;
            throw std::runtime_error("Failed to create Vulkan depth image view.");
        }

        QUIET_CORE_INFO(
            "Created depth resources. format={} extent={}x{}",
            static_cast<int>(m_Format),
            m_Extent.width,
            m_Extent.height
        );
    }

    DepthResources::~DepthResources()
    {
        if (m_ImageView != VK_NULL_HANDLE)
        {
            vkDestroyImageView(m_Device, m_ImageView, nullptr);
            m_ImageView = VK_NULL_HANDLE;
        }

        if (m_Image != VK_NULL_HANDLE)
        {
            vkDestroyImage(m_Device, m_Image, nullptr);
            m_Image = VK_NULL_HANDLE;
        }

        if (m_Memory != VK_NULL_HANDLE)
        {
            vkFreeMemory(m_Device, m_Memory, nullptr);
            m_Memory = VK_NULL_HANDLE;
        }
    }
}

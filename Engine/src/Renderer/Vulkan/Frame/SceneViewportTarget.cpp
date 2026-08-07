#include "Renderer/Vulkan/Frame/SceneViewportTarget.h"

#include <stdexcept>

#include "Quiet/Core/Log.h"

namespace Quiet::Renderer::Vulkan
{
    SceneViewportTarget::SceneViewportTarget(VkPhysicalDevice physicalDevice,
                                             VkDevice device,
                                             VkFormat colorFormat,
                                             VkFormat depthFormat)
        : m_PhysicalDevice(physicalDevice)
        , m_Device(device)
        , m_ColorFormat(colorFormat)
        , m_DepthFormat(depthFormat) {}

    SceneViewportTarget::~SceneViewportTarget()
    {
        Destroy();
    }

    void SceneViewportTarget::Resize(std::uint32_t width, std::uint32_t height)
    {
        if (width == 0 || height == 0)
        {
            Destroy();
            return;
        }
        if (m_Extent.width == width && m_Extent.height == height && IsValid())
        {
            return;
        }

        Destroy();

        m_Extent.width = width;
        m_Extent.height = height;

        CreateColorResources(width, height);
        CreateDepthResources(width, height);
        CreateSelectionMaskResources(width, height);
        CreateSampler();

        QUIET_CORE_INFO("Created scene viewport target. extent={}x{}", width, height);
    }

    void SceneViewportTarget::Destroy()
    {
        if (m_Sampler != VK_NULL_HANDLE)
        {
            vkDestroySampler(m_Device, m_Sampler, nullptr);
            m_Sampler = VK_NULL_HANDLE;
        }

        if (m_SelectionMaskImageView != VK_NULL_HANDLE)
        {
            vkDestroyImageView(m_Device, m_SelectionMaskImageView, nullptr);
            m_SelectionMaskImageView = VK_NULL_HANDLE;
        }

        if (m_SelectionMaskImage != VK_NULL_HANDLE)
        {
            vkDestroyImage(m_Device, m_SelectionMaskImage, nullptr);
            m_SelectionMaskImage = VK_NULL_HANDLE;
        }

        if (m_SelectionMaskMemory != VK_NULL_HANDLE)
        {
            vkFreeMemory(m_Device, m_SelectionMaskMemory, nullptr);
            m_SelectionMaskMemory = VK_NULL_HANDLE;
        }

        if (m_ColorImageView != VK_NULL_HANDLE)
        {
            vkDestroyImageView(m_Device, m_ColorImageView, nullptr);
            m_ColorImageView = VK_NULL_HANDLE;
        }

        if (m_ColorImage != VK_NULL_HANDLE)
        {
            vkDestroyImage(m_Device, m_ColorImage, nullptr);
            m_ColorImage = VK_NULL_HANDLE;
        }

        if (m_ColorMemory != VK_NULL_HANDLE)
        {
            vkFreeMemory(m_Device, m_ColorMemory, nullptr);
            m_ColorMemory = VK_NULL_HANDLE;
        }

        if (m_DepthImageView != VK_NULL_HANDLE)
        {
            vkDestroyImageView(m_Device, m_DepthImageView, nullptr);
            m_DepthImageView = VK_NULL_HANDLE;
        }

        if (m_DepthImage != VK_NULL_HANDLE)
        {
            vkDestroyImage(m_Device, m_DepthImage, nullptr);
            m_DepthImage = VK_NULL_HANDLE;
        }

        if (m_DepthMemory != VK_NULL_HANDLE)
        {
            vkFreeMemory(m_Device, m_DepthMemory, nullptr);
            m_DepthMemory = VK_NULL_HANDLE;
        }

        m_ColorLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        m_DepthLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        m_SelectionMaskLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        m_Extent = {};
    }

    void SceneViewportTarget::CreateColorResources(std::uint32_t width, std::uint32_t height)
    {
        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.format = m_ColorFormat;
        imageInfo.extent.width = width;
        imageInfo.extent.height = height;
        imageInfo.extent.depth = 1;
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        if (vkCreateImage(m_Device, &imageInfo, nullptr, &m_ColorImage) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create scene viewport color image.");
        }

        VkMemoryRequirements memRequirements{};
        vkGetImageMemoryRequirements(m_Device, m_ColorImage, &memRequirements);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = FindMemoryType(
            memRequirements.memoryTypeBits,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
        );

        if (vkAllocateMemory(m_Device, &allocInfo, nullptr, &m_ColorMemory) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to allocate scene viewport color image memory.");
        }

        if (vkBindImageMemory(m_Device, m_ColorImage, m_ColorMemory, 0) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to bind scene viewport color image memory.");
        }

        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = m_ColorImage;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = m_ColorFormat;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        if (vkCreateImageView(m_Device, &viewInfo, nullptr, &m_ColorImageView) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create scene viewport color image view.");
        }

        m_ColorLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    }

    void SceneViewportTarget::CreateDepthResources(std::uint32_t width, std::uint32_t height)
    {
        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.format = m_DepthFormat;
        imageInfo.extent.width = width;
        imageInfo.extent.height = height;
        imageInfo.extent.depth = 1;
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        if (vkCreateImage(m_Device, &imageInfo, nullptr, &m_DepthImage) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create scene viewport depth image.");
        }

        VkMemoryRequirements memRequirements{};
        vkGetImageMemoryRequirements(m_Device, m_DepthImage, &memRequirements);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = FindMemoryType(
            memRequirements.memoryTypeBits,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
        );

        if (vkAllocateMemory(m_Device, &allocInfo, nullptr, &m_DepthMemory) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to allocate scene viewport depth image memory.");
        }

        if (vkBindImageMemory(m_Device, m_DepthImage, m_DepthMemory, 0) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to bind scene viewport depth image memory.");
        }

        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = m_DepthImage;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = m_DepthFormat;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        if (vkCreateImageView(m_Device, &viewInfo, nullptr, &m_DepthImageView) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create scene viewport depth image view.");
        }

        m_DepthLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    }

    void SceneViewportTarget::CreateSelectionMaskResources(std::uint32_t width, std::uint32_t height)
    {
        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.format = kSelectionMaskFormat;
        imageInfo.extent.width = width;
        imageInfo.extent.height = height;
        imageInfo.extent.depth = 1;
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        if (vkCreateImage(m_Device, &imageInfo, nullptr, &m_SelectionMaskImage) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create scene viewport selection-mask image.");
        }

        VkMemoryRequirements memoryRequirements{};
        vkGetImageMemoryRequirements(m_Device, m_SelectionMaskImage, &memoryRequirements);

        VkMemoryAllocateInfo allocationInfo{};
        allocationInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocationInfo.allocationSize = memoryRequirements.size;
        allocationInfo.memoryTypeIndex = FindMemoryType(memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        if (vkAllocateMemory(m_Device, &allocationInfo, nullptr, &m_SelectionMaskMemory) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to allocate scene viewport selection-mask image memory.");
        }

        if (vkBindImageMemory(m_Device, m_SelectionMaskImage, m_SelectionMaskMemory, 0) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to bind scene viewport selection-mask image memory.");
        }

        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = m_SelectionMaskImage;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = kSelectionMaskFormat;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel =  0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        if (vkCreateImageView(m_Device, &viewInfo, nullptr, &m_SelectionMaskImageView) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create scene viewport selection-mask image view.");
        }

        m_SelectionMaskLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    }

    void SceneViewportTarget::CreateSampler()
    {
        VkSamplerCreateInfo samplerInfo{};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.mipLodBias = 0.0f;
        samplerInfo.maxAnisotropy = 1.0f;
        samplerInfo.anisotropyEnable = VK_FALSE;
        samplerInfo.compareEnable = VK_FALSE;
        samplerInfo.minLod = 0.0f;
        samplerInfo.maxLod = 1.0f;
        samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        samplerInfo.unnormalizedCoordinates = VK_FALSE;

        if (vkCreateSampler(m_Device, &samplerInfo, nullptr, &m_Sampler) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create scene viewport sampler.");
        }
    }

    std::uint32_t SceneViewportTarget::FindMemoryType(std::uint32_t typeFilter, VkMemoryPropertyFlags properties) const
    {
        VkPhysicalDeviceMemoryProperties memProperties{};
        vkGetPhysicalDeviceMemoryProperties(m_PhysicalDevice, &memProperties);

        for (std::uint32_t i = 0; i < memProperties.memoryTypeCount; ++i)
        {
            const bool matchesType = (typeFilter & (1u << i)) != 0;
            const bool matchesProps =
                (memProperties.memoryTypes[i].propertyFlags & properties) == properties;

            if (matchesType && matchesProps)
            {
                return i;
            }
        }

        throw std::runtime_error("Failed to find suitable memory type for scene viewport target.");
    }

}

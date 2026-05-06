#include "Renderer/Vulkan/Texture/Texture2D.h"

#include <cstring>
#include <stdexcept>

#include "Quiet/Core/Log.h"
#include "Renderer/Vulkan/GraphicsCommands.h"
#include "Renderer/Vulkan/LogicalDevice.h"
#include "Renderer/Vulkan/PhysicalDevice.h"
#include "Renderer/Vulkan/VulkanBuffer.h"
#include "Renderer/Vulkan/Texture/TexturePixelData.h"

namespace
{
    VkCommandBuffer BeginOneShotCommands(VkDevice device, VkCommandPool commandPool)
    {
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = commandPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = 1;

        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        if (vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer) != VK_SUCCESS)
            throw std::runtime_error("Failed to allocate texture upload command buffer.");

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS)
        {
            vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
            throw std::runtime_error("Failed to begin texture upload command buffer.");
        }

        return commandBuffer;
    }

    void EndOneShotCommands(VkDevice device, VkCommandPool commandPool, VkQueue queue, VkCommandBuffer commandBuffer)
    {
        if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS)
        {
            vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
            throw std::runtime_error("Failed to end texture upload command buffer.");
        }

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;

        if (vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE) != VK_SUCCESS)
        {
            vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
            throw std::runtime_error("Failed to submit texture upload command buffer.");
        }

        vkQueueWaitIdle(queue);
        vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
    }

    void TransitionImageLayout(VkCommandBuffer commandBuffer, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout)
    {
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

        VkPipelineStageFlags srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        VkPipelineStageFlags dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;

        if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED &&
            newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
        {
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

            srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        }
        else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
                 newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
        {
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

            srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        }
        else
        {
            throw std::runtime_error("Unsupported texture image layout transition.");
        }

        vkCmdPipelineBarrier(
            commandBuffer,
            srcStage,
            dstStage,
            0,
            0,
            nullptr,
            0,
            nullptr,
            1,
            &barrier
        );
    }

    void CopyBufferToImage(VkCommandBuffer commandBuffer, VkBuffer buffer, VkImage image, std::uint32_t width, std::uint32_t height)
    {
        VkBufferImageCopy region{};
        region.bufferOffset = 0;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;

        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount = 1;

        region.imageOffset = { 0, 0, 0 };
        region.imageExtent = { width, height, 1 };

        vkCmdCopyBufferToImage(
            commandBuffer,
            buffer,
            image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1,
            &region
        );
    }
}

namespace Quiet::Renderer::Vulkan
{
    Texture2D::Texture2D(const PhysicalDevice &physicalDevice,
                         const LogicalDevice &logicalDevice,
                         const GraphicsCommands &graphicsCommands,
                         const TexturePixelData &pixelData)
                             : m_Device(logicalDevice.GetHandle())
                             , m_Width(pixelData.Width)
                             , m_Height(pixelData.Height)
    {
        if (!pixelData.IsValid())
            throw std::runtime_error("Texture2D cannot be created from invalid pixel data.");

        const auto imageSize = static_cast<VkDeviceSize>(pixelData.Pixels.size());

        VulkanBuffer stagingBuffer(
            physicalDevice,
            logicalDevice,
            imageSize,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
        );

        void* mapped = stagingBuffer.Map();
        std::memcpy(mapped, pixelData.Pixels.data(), pixelData.Pixels.size());
        stagingBuffer.Unmap();

        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent.width = m_Width;
        imageInfo.extent.height = m_Height;
        imageInfo.extent.depth = 1;
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.format = m_Format;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        if (vkCreateImage(m_Device, &imageInfo, nullptr, &m_Image) != VK_SUCCESS)
            throw std::runtime_error("Failed to create texture image.");

        VkMemoryRequirements memoryRequirements;
        vkGetImageMemoryRequirements(m_Device, m_Image, &memoryRequirements);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memoryRequirements.size;
        allocInfo.memoryTypeIndex = physicalDevice.FindMemoryType(
            memoryRequirements.memoryTypeBits,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
        );

        if (vkAllocateMemory(m_Device, &allocInfo, nullptr, &m_Memory) != VK_SUCCESS)
        {
            vkDestroyImage(m_Device, m_Image, nullptr);
            m_Image = VK_NULL_HANDLE;
            throw std::runtime_error("Failed to allocate texture image memory.");
        }

        if (vkBindImageMemory(m_Device, m_Image, m_Memory, 0) != VK_SUCCESS)
        {
            vkFreeMemory(m_Device, m_Memory, nullptr);
            vkDestroyImage(m_Device, m_Image, nullptr);

            m_Memory = VK_NULL_HANDLE;
            m_Image = VK_NULL_HANDLE;

            throw std::runtime_error("Failed to bind texture image memory.");
        }

        try
        {
            VkCommandBuffer commandBuffer = BeginOneShotCommands(
                m_Device,
                graphicsCommands.GetCommandPool()
            );

            TransitionImageLayout(
                commandBuffer,
                m_Image,
                VK_IMAGE_LAYOUT_UNDEFINED,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
            );

            CopyBufferToImage(
                commandBuffer,
                stagingBuffer.GetHandle(),
                m_Image,
                m_Width,
                m_Height
            );

            TransitionImageLayout(
                commandBuffer,
                m_Image,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            );

            EndOneShotCommands(
                m_Device,
                graphicsCommands.GetCommandPool(),
                logicalDevice.GetGraphicsQueue(),
                commandBuffer
            );

            VkImageViewCreateInfo viewInfo{};
            viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            viewInfo.image = m_Image;
            viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            viewInfo.format = m_Format;
            viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            viewInfo.subresourceRange.baseMipLevel = 0;
            viewInfo.subresourceRange.levelCount = 1;
            viewInfo.subresourceRange.baseArrayLayer = 0;
            viewInfo.subresourceRange.layerCount = 1;

            if (vkCreateImageView(m_Device, &viewInfo, nullptr, &m_ImageView) != VK_SUCCESS)
            {
                throw std::runtime_error("Failed to create texture image view.");
            }

            VkSamplerCreateInfo samplerInfo{};
            samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
            samplerInfo.magFilter = VK_FILTER_LINEAR;
            samplerInfo.minFilter = VK_FILTER_LINEAR;
            samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
            samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
            samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
            samplerInfo.anisotropyEnable = VK_FALSE;
            samplerInfo.maxAnisotropy = 1.0f;
            samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
            samplerInfo.unnormalizedCoordinates = VK_FALSE;
            samplerInfo.compareEnable = VK_FALSE;
            samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
            samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
            samplerInfo.mipLodBias = 0.0f;
            samplerInfo.minLod = 0.0f;
            samplerInfo.maxLod = 0.0f;

            if (vkCreateSampler(m_Device, &samplerInfo, nullptr, &m_Sampler) != VK_SUCCESS)
            {
                throw std::runtime_error("Failed to create texture sampler.");
            }
        }
        catch (...)
        {
            Destroy();
            throw;
        }

        QUIET_CORE_INFO(
            "Created Texture2D. size={}x{} bytes={}",
            m_Width,
            m_Height,
            pixelData.Pixels.size()
        );
    }

    Texture2D::~Texture2D()
    {
        Destroy();
    }

    void Texture2D::Destroy()
    {
        if (m_Sampler != VK_NULL_HANDLE)
        {
            vkDestroySampler(m_Device, m_Sampler, nullptr);
            m_Sampler = VK_NULL_HANDLE;
        }
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

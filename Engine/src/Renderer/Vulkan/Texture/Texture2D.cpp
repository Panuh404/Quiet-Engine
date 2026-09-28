#include "Renderer/Vulkan/Texture/Texture2D.h"

#include <algorithm>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <vector>

#include "Quiet/Assets/QTextureReader.h"
#include "Quiet/Assets/TexturePixelData.h"
#include "Quiet/Core/Log.h"
#include "Renderer/Vulkan/Context/GraphicsCommands.h"
#include "Renderer/Vulkan/Context/LogicalDevice.h"
#include "Renderer/Vulkan/Context/PhysicalDevice.h"
#include "Renderer/Vulkan/Context/VulkanBuffer.h"

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

    void TransitionImageLayout(VkCommandBuffer commandBuffer, VkImage image, const std::uint32_t mipLevels, VkImageLayout oldLayout, VkImageLayout newLayout)
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
        barrier.subresourceRange.levelCount = mipLevels;
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

    void CopyBufferToImage(VkCommandBuffer commandBuffer, VkBuffer buffer, VkImage image, const std::span<const VkBufferImageCopy> regions)
    {
        if (regions.empty())
            throw std::runtime_error("Cannot upload a texture without copy regions.");

        vkCmdCopyBufferToImage(
            commandBuffer,
            buffer,
            image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            static_cast<std::uint32_t>(regions.size()),
            regions.data());
    }

    VkDeviceSize GetExpectedMipByteSize(const VkFormat format, const std::uint32_t width, const std::uint32_t height)
    {
        switch (format)
        {
            case VK_FORMAT_R8G8B8A8_SRGB:
            {
                return static_cast<VkDeviceSize>(width) * static_cast<VkDeviceSize>(height) * 4;
            }

            case VK_FORMAT_BC7_SRGB_BLOCK:
            {
                const VkDeviceSize blocksX = (static_cast<VkDeviceSize>(width) + 3) / 4;
                const VkDeviceSize blocksY = (static_cast<VkDeviceSize>(height) + 3) / 4;
                return blocksX * blocksY * 16;
            }

            default:
                throw std::runtime_error("Texture2D does not support this Vulkan texture format.");
        }
    }
}

namespace Quiet::Renderer::Vulkan
{
    Texture2D::Texture2D(
        const PhysicalDevice& physicalDevice,
        const LogicalDevice& logicalDevice,
        const GraphicsCommands& graphicsCommands,
        const Assets::TexturePixelData& pixelData)
        : m_Device(logicalDevice.GetHandle())
    {
        if (!pixelData.IsValid())
            throw std::runtime_error("Texture2D cannot be created from invalid pixel data.");

        const UploadMip mip
        {
            pixelData.Width,
            pixelData.Height,
            std::span<const std::uint8_t>(pixelData.Pixels.data(), pixelData.Pixels.size())
        };

        Create(physicalDevice, logicalDevice, graphicsCommands, VK_FORMAT_R8G8B8A8_SRGB, std::span<const UploadMip>(&mip, 1));
    }

    Texture2D::Texture2D(
        const PhysicalDevice& physicalDevice,
        const LogicalDevice& logicalDevice,
        const GraphicsCommands& graphicsCommands,
        const Assets::QTextureData& textureData)
            : m_Device(logicalDevice.GetHandle())
    {
        if (!textureData.IsValid())
            throw std::runtime_error("Texture2D cannot be created from invalid QTexture data.");

        const VkFormat format = static_cast<VkFormat>(textureData.VulkanFormat);

        // Current cooked texture policy.
        //
        // Future BC5/BC4 support should be added explicitly rather
        // than silently treating every KTX2 VkFormat as supported.
        if (format != VK_FORMAT_BC7_SRGB_BLOCK)
            throw std::runtime_error("Texture2D currently supports only BC7 sRGB cooked QTextures.");

        std::vector<UploadMip> mipLevels;
        mipLevels.reserve(textureData.MipLevels.size());

        for (const Assets::QTextureMipData& mip : textureData.MipLevels)
        {
            mipLevels.push_back(UploadMip
            {
                mip.Width,
                mip.Height,
                std::span<const std::uint8_t>(mip.Data.data(), mip.Data.size())
            });
        }

        Create(physicalDevice, logicalDevice, graphicsCommands, format, mipLevels);
    }

    Texture2D::~Texture2D()
    {
        Destroy();
    }

    VkDescriptorImageInfo Texture2D::GetDescriptorImageInfo() const
    {
        VkDescriptorImageInfo imageInfo{};
        imageInfo.sampler = m_Sampler;
        imageInfo.imageView = m_ImageView;
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        return imageInfo;
    }

    void Texture2D::Create(
        const PhysicalDevice& physicalDevice,
        const LogicalDevice& logicalDevice,
        const GraphicsCommands& graphicsCommands,
        const VkFormat format,
        const std::span<const UploadMip> mipLevels)
    {
        if (mipLevels.empty())
            throw std::runtime_error("Texture2D cannot be created without mip levels.");

        if (mipLevels.size() > std::numeric_limits<std::uint32_t>::max())
            throw std::runtime_error("Texture2D mip count exceeds Vulkan limits.");

        m_Format = format;
        m_Width = mipLevels.front().Width;
        m_Height = mipLevels.front().Height;
        m_MipLevels = static_cast<std::uint32_t>(mipLevels.size());

        if (m_Width == 0 || m_Height == 0)
            throw std::runtime_error("Texture2D base dimensions are invalid.");

        VkFormatProperties formatProperties{};

        vkGetPhysicalDeviceFormatProperties(physicalDevice.GetHandle(), m_Format, &formatProperties);

        constexpr VkFormatFeatureFlags RequiredFeatures = VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT | VK_FORMAT_FEATURE_TRANSFER_DST_BIT;
        if ((formatProperties.optimalTilingFeatures & RequiredFeatures) != RequiredFeatures)
            throw std::runtime_error("Selected GPU does not support the texture format for optimal sampled transfer-destination images.");

        std::vector<VkBufferImageCopy> copyRegions;
        copyRegions.reserve(mipLevels.size());

        std::size_t stagingSize = 0;
        std::uint32_t expectedWidth = m_Width;
        std::uint32_t expectedHeight = m_Height;

        for (std::size_t level = 0; level < mipLevels.size(); ++level)
        {
            const UploadMip& mip = mipLevels[level];
            if (mip.Width != expectedWidth || mip.Height != expectedHeight)
                throw std::runtime_error("Texture2D mip dimensions do not form a valid chain.");

            const VkDeviceSize expectedByteSize = GetExpectedMipByteSize(m_Format, mip.Width, mip.Height);
            if (expectedByteSize != mip.Data.size())
                throw std::runtime_error("Texture2D mip byte size does not match its Vulkan format.");

            if (mip.Data.size() > std::numeric_limits<std::size_t>::max() - stagingSize)
                throw std::runtime_error("Texture2D staging data exceeds addressable memory.");

            VkBufferImageCopy region{};
            region.bufferOffset = static_cast<VkDeviceSize>(stagingSize);
            region.bufferRowLength = 0;
            region.bufferImageHeight = 0;
            region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            region.imageSubresource.mipLevel = static_cast<std::uint32_t>(level);
            region.imageSubresource.baseArrayLayer = 0;
            region.imageSubresource.layerCount = 1;
            region.imageOffset = { 0, 0, 0 };
            region.imageExtent = { mip.Width, mip.Height, 1 };

            copyRegions.push_back(region);
            stagingSize += mip.Data.size();

            expectedWidth = std::max(1u, expectedWidth / 2);
            expectedHeight = std::max(1u, expectedHeight / 2);
        }

        if (stagingSize == 0)
            throw std::runtime_error("Texture2D contains no upload data.");

        VulkanBuffer stagingBuffer(
            physicalDevice,
            logicalDevice,
            static_cast<VkDeviceSize>(stagingSize),
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        auto* mapped = static_cast<std::uint8_t*>(stagingBuffer.Map());

        for (std::size_t level = 0; level < mipLevels.size(); ++level)
        {
            const UploadMip& mip = mipLevels[level];
            const std::size_t destinationOffset = static_cast<std::size_t>(copyRegions[level].bufferOffset);

            std::memcpy(mapped + destinationOffset, mip.Data.data(), mip.Data.size());
        }

        stagingBuffer.Unmap();

        try
        {
            VkImageCreateInfo imageInfo{};
            imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
            imageInfo.imageType = VK_IMAGE_TYPE_2D;
            imageInfo.extent.width = m_Width;
            imageInfo.extent.height = m_Height;
            imageInfo.extent.depth = 1;
            imageInfo.mipLevels = m_MipLevels;
            imageInfo.arrayLayers = 1;
            imageInfo.format = m_Format;
            imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
            imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
            imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
            imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

            if (vkCreateImage(m_Device, &imageInfo, nullptr, &m_Image) != VK_SUCCESS)
                throw std::runtime_error("Failed to create texture image.");

            VkMemoryRequirements memoryRequirements{};
            vkGetImageMemoryRequirements(m_Device, m_Image, &memoryRequirements);

            VkMemoryAllocateInfo allocInfo{};
            allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
            allocInfo.allocationSize = memoryRequirements.size;
            allocInfo.memoryTypeIndex = physicalDevice.FindMemoryType(memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

            if (vkAllocateMemory(m_Device, &allocInfo, nullptr, &m_Memory) != VK_SUCCESS)
                throw std::runtime_error("Failed to allocate texture image memory.");

            if (vkBindImageMemory(m_Device, m_Image, m_Memory, 0) != VK_SUCCESS)
                throw std::runtime_error("Failed to bind texture image memory.");

            VkCommandBuffer commandBuffer = BeginOneShotCommands(m_Device, graphicsCommands.GetCommandPool());
            TransitionImageLayout(
                commandBuffer,
                m_Image,
                m_MipLevels,
                VK_IMAGE_LAYOUT_UNDEFINED,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

            CopyBufferToImage(commandBuffer, stagingBuffer.GetHandle(), m_Image, copyRegions);

            TransitionImageLayout(
                commandBuffer,
                m_Image,
                m_MipLevels,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

            EndOneShotCommands(
                m_Device,
                graphicsCommands.GetCommandPool(),
                logicalDevice.GetGraphicsQueue(),
                commandBuffer);

            VkImageViewCreateInfo viewInfo{};
            viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            viewInfo.image = m_Image;
            viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            viewInfo.format = m_Format;
            viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            viewInfo.subresourceRange.baseMipLevel = 0;
            viewInfo.subresourceRange.levelCount = m_MipLevels;
            viewInfo.subresourceRange.baseArrayLayer = 0;
            viewInfo.subresourceRange.layerCount = 1;

            if (vkCreateImageView(m_Device, &viewInfo, nullptr, &m_ImageView) != VK_SUCCESS)
                throw std::runtime_error("Failed to create texture image view.");

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
            samplerInfo.maxLod = static_cast<float>(m_MipLevels - 1);

            if (vkCreateSampler(m_Device, &samplerInfo, nullptr, &m_Sampler) != VK_SUCCESS)
                throw std::runtime_error("Failed to create texture sampler.");
        }
        catch (...)
        {
            Destroy();
            throw;
        }

        QUIET_CORE_INFO("Created Texture2D. size={}x{} mips={} format={} bytes={}",
            m_Width, m_Height, m_MipLevels, static_cast<std::uint32_t>(m_Format), stagingSize);
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

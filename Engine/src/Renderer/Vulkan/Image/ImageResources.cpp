#include "Renderer/Vulkan/Image/ImageResources.h"

#include <stdexcept>
#include <utility>

#include "Renderer/Vulkan/Context/LogicalDevice.h"
#include "Renderer/Vulkan/Texture/Texture2D.h"

namespace Quiet::Renderer::Vulkan
{
    ImageResources::ImageResources(const LogicalDevice& logicalDevice, const VkDescriptorSetLayout descriptorSetLayout)
        : m_Device(logicalDevice.GetHandle()), m_DescriptorSetLayout(descriptorSetLayout)
    {
        if (m_DescriptorSetLayout == VK_NULL_HANDLE)
        {
            throw std::invalid_argument("Image resources require a descriptor-set layout.");
        }
    }

    ImageResources::~ImageResources()
    {
        if (m_DescriptorPool != VK_NULL_HANDLE)
        {
            vkDestroyDescriptorPool(m_Device, m_DescriptorPool, nullptr);
        }
    }

    void ImageResources::BeginFrame(const std::uint32_t requiredSets)
    {
        // Reserve before changing Vulkan resources. Capacity never shrinks.
        m_RetainedTextures.reserve(requiredSets);

        if (requiredSets > m_Capacity)
        {
            VkDescriptorPoolSize poolSize{};
            poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            poolSize.descriptorCount = requiredSets;

            VkDescriptorPoolCreateInfo poolInfo{};
            poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
            poolInfo.maxSets = requiredSets;
            poolInfo.poolSizeCount = 1;
            poolInfo.pPoolSizes = &poolSize;

            VkDescriptorPool replacementPool = VK_NULL_HANDLE;

            if (vkCreateDescriptorPool(m_Device, &poolInfo, nullptr, &replacementPool) != VK_SUCCESS)
            {
                throw std::runtime_error("Failed to create image descriptor pool.");
            }

            if (m_DescriptorPool != VK_NULL_HANDLE)
            {
                vkDestroyDescriptorPool(m_Device, m_DescriptorPool, nullptr);
            }

            m_DescriptorPool = replacementPool;
            m_Capacity = requiredSets;
        }
        else if (m_DescriptorPool != VK_NULL_HANDLE)
        {
            if (vkResetDescriptorPool(m_Device, m_DescriptorPool, 0) != VK_SUCCESS)
            {
                throw std::runtime_error("Failed to reset image descriptor pool.");
            }
        }
        m_RetainedTextures.clear();
    }

    VkDescriptorSet ImageResources::CreateBinding(std::shared_ptr<Texture2D> texture)
    {
        if (!texture) return VK_NULL_HANDLE;
        if (texture->GetImageView() == VK_NULL_HANDLE || texture->GetSampler() == VK_NULL_HANDLE)
        {
            throw std::invalid_argument("Image binding requires an initialized texture.");
        }
        if (m_DescriptorPool == VK_NULL_HANDLE || m_RetainedTextures.size() >= m_Capacity)
        {
            throw std::runtime_error("Image descriptor capacity was not prepared for this frame.");
        }

        VkDescriptorSetAllocateInfo allocationInfo{};
        allocationInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocationInfo.descriptorPool = m_DescriptorPool;
        allocationInfo.descriptorSetCount = 1;
        allocationInfo.pSetLayouts = &m_DescriptorSetLayout;

        VkDescriptorSet descriptorSet = VK_NULL_HANDLE;

        if (vkAllocateDescriptorSets(m_Device, &allocationInfo, &descriptorSet) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to allocate image descriptor set.");
        }

        const VkDescriptorImageInfo imageInfo = texture->GetDescriptorImageInfo();

        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = descriptorSet;
        write.dstBinding = 0;
        write.descriptorCount = 1;
        write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        write.pImageInfo = &imageInfo;

        vkUpdateDescriptorSets(m_Device, 1, &write, 0, nullptr);

        m_RetainedTextures.push_back(std::move(texture));
        return descriptorSet;
    }
}
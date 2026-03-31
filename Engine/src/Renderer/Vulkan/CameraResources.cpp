#include "CameraResources.h"

#include <cstring>

#include "LogicalDevice.h"
#include "Swapchain.h"
#include "TriangleTypes.h"
#include "VulkanBuffer.h"

namespace Quiet::Renderer::Vulkan
{
    CameraResources::CameraResources(const PhysicalDevice &physicalDevice,
                                     const LogicalDevice &logicalDevice,
                                     const Swapchain &swapchain)
        : m_Device(logicalDevice.GetHandle())
    {
        VkDescriptorSetLayoutBinding cameraBinding{};
        cameraBinding.binding = 0;
        cameraBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        cameraBinding.descriptorCount = 1;
        cameraBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        cameraBinding.pImmutableSamplers = nullptr;

        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = 1;
        layoutInfo.pBindings = &cameraBinding;

        if (vkCreateDescriptorSetLayout(m_Device, &layoutInfo, nullptr, &m_DescriptorSetLayout) != VK_SUCCESS)
            throw std::runtime_error("Failed to create camera descriptor set layout.");

        const std::size_t imageCount = swapchain.GetImages().size();
        m_UniformBuffers.reserve(imageCount);
        m_MappedUniformBuffers.resize(imageCount, nullptr);

        for (std::size_t i = 0; i < imageCount; ++i)
        {
            auto buffer = std::make_unique<VulkanBuffer>(
                physicalDevice,
                logicalDevice,
                sizeof(CameraUniformData),
                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
            );
            m_MappedUniformBuffers[i] = buffer->Map();
            m_UniformBuffers.push_back(std::move(buffer));
        }

        VkDescriptorPoolSize poolSize{};
        poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        poolSize.descriptorCount = static_cast<std::uint32_t>(imageCount);

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = 1;
        poolInfo.pPoolSizes = &poolSize;
        poolInfo.maxSets = static_cast<std::uint32_t>(imageCount);

        if (vkCreateDescriptorPool(m_Device, &poolInfo, nullptr, &m_DescriptorPool) != VK_SUCCESS)
            throw std::runtime_error("Failed to create camera descriptor pool.");

        std::vector<VkDescriptorSetLayout> layouts(imageCount, m_DescriptorSetLayout);

        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = m_DescriptorPool;
        allocInfo.descriptorSetCount = static_cast<std::uint32_t>(imageCount);
        allocInfo.pSetLayouts = layouts.data();

        m_DescriptorSets.resize(imageCount);
        if (vkAllocateDescriptorSets(m_Device, &allocInfo, m_DescriptorSets.data()) != VK_SUCCESS)
            throw std::runtime_error("Failed to allocate camera descriptor sets.");

        for (std::size_t i = 0; i < imageCount; ++i)
        {
            VkDescriptorBufferInfo bufferInfo{};
            bufferInfo.buffer = m_UniformBuffers[i]->GetHandle();
            bufferInfo.offset = 0;
            bufferInfo.range = sizeof(CameraUniformData);

            VkWriteDescriptorSet write{};
            write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write.dstSet = m_DescriptorSets[i];
            write.dstBinding = 0;
            write.dstArrayElement = 0;
            write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            write.descriptorCount = 1;
            write.pBufferInfo = &bufferInfo;

            vkUpdateDescriptorSets(m_Device, 1, &write, 0, nullptr);
        }
    }

    CameraResources::~CameraResources()
    {
        for (std::size_t i = 0; i < m_UniformBuffers.size(); ++i)
        {
            if (m_UniformBuffers[i] && m_MappedUniformBuffers[i] != nullptr)
            {
                m_UniformBuffers[i]->Unmap();
                m_MappedUniformBuffers[i] = nullptr;
            }
        }
        m_UniformBuffers.clear();
        if (m_DescriptorPool != VK_NULL_HANDLE)
        {
            vkDestroyDescriptorPool(m_Device, m_DescriptorPool, nullptr);
            m_DescriptorPool = VK_NULL_HANDLE;
        }
        if (m_DescriptorSetLayout != VK_NULL_HANDLE)
        {
            vkDestroyDescriptorSetLayout(m_Device, m_DescriptorSetLayout, nullptr);
            m_DescriptorSetLayout = VK_NULL_HANDLE;
        }
    }

    void CameraResources::Update(std::uint32_t imageIndex, const CameraUniformData &data)
    {
        std::memcpy(m_MappedUniformBuffers.at(imageIndex), &data, sizeof(CameraUniformData));
    }
}

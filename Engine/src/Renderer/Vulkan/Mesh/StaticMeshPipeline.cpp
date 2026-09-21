#include "Renderer/Vulkan/Mesh/StaticMeshPipeline.h"

#include <cstdint>
#include <fstream>
#include <stdexcept>

#include "Quiet/Core/Log.h"
#include "Renderer/Vulkan/Context/LogicalDevice.h"
#include "Renderer/Vulkan/Frame/Swapchain.h"
#include "Renderer/Vulkan/Frame/DepthResources.h"
#include "Renderer/Vulkan/Mesh/StaticMesh.h"
#include "Renderer/Vulkan/RenderTypes.h"
#include "Renderer/Vulkan/Shader/ShaderModule.h"


namespace Quiet::Renderer::Vulkan
{
    StaticMeshPipeline::StaticMeshPipeline(const LogicalDevice& logicalDevice,
                                           VkFormat colorAttachmentFormat,
                                           VkFormat depthAttachmentFormat,
                                           VkDescriptorSetLayout cameraSetLayout,
                                           VkDescriptorSetLayout materialSetLayout)
        : m_Device(logicalDevice.GetHandle())
    {
        ShaderModule vertShader(m_Device, "Resources/Shaders/StaticMesh.vert.spv");
        ShaderModule fragShader(m_Device, "Resources/Shaders/StaticMesh.frag.spv");

        VkPipelineShaderStageCreateInfo stages[] = {
            vertShader.MakeStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
            fragShader.MakeStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT),
        };

        VkPushConstantRange pushConstantRange{};
        pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        pushConstantRange.offset = 0;
        pushConstantRange.size = sizeof(StaticMeshPushConstants);

        const VkDescriptorSetLayout setLayouts[] =
        {
            cameraSetLayout,
            materialSetLayout
        };

        VkPipelineLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layoutInfo.setLayoutCount = 2;
        layoutInfo.pSetLayouts = setLayouts;
        layoutInfo.pushConstantRangeCount = 1;
        layoutInfo.pPushConstantRanges = &pushConstantRange;

        if (vkCreatePipelineLayout(m_Device, &layoutInfo, nullptr, &m_PipelineLayout) != VK_SUCCESS)
            throw std::runtime_error("Failed to create static mesh pipeline layout.");

        const auto bindingDescription = GetStaticMeshVertexBindingDescription();
        const auto attributeDescriptions = GetStaticMeshVertexAttributeDescriptions();

        VkPipelineVertexInputStateCreateInfo vertexInput{};
        vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInput.vertexBindingDescriptionCount = 1;
        vertexInput.pVertexBindingDescriptions = &bindingDescription;
        vertexInput.vertexAttributeDescriptionCount = static_cast<std::uint32_t>(attributeDescriptions.size());
        vertexInput.pVertexAttributeDescriptions = attributeDescriptions.data();

        VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        inputAssembly.primitiveRestartEnable = VK_FALSE;

        VkPipelineViewportStateCreateInfo viewportState{};
        viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1;
        viewportState.scissorCount = 1;

        VkPipelineRasterizationStateCreateInfo rasterizer{};
        rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizer.depthClampEnable = VK_FALSE;
        rasterizer.rasterizerDiscardEnable = VK_FALSE;
        rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizer.lineWidth = 1.0f;
        rasterizer.cullMode = VK_CULL_MODE_NONE;
        rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        rasterizer.depthBiasEnable = VK_FALSE;

        VkPipelineMultisampleStateCreateInfo multisample{};
        multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        VkPipelineDepthStencilStateCreateInfo depthStencil{};
        depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depthStencil.depthTestEnable = VK_TRUE;
        depthStencil.depthWriteEnable = VK_TRUE;
        depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
        depthStencil.depthBoundsTestEnable = VK_FALSE;
        depthStencil.stencilTestEnable = VK_FALSE;

        VkPipelineColorBlendAttachmentState colorBlendAttachment{};
        colorBlendAttachment.colorWriteMask =
            VK_COLOR_COMPONENT_R_BIT |
            VK_COLOR_COMPONENT_G_BIT |
            VK_COLOR_COMPONENT_B_BIT |
            VK_COLOR_COMPONENT_A_BIT;
        colorBlendAttachment.blendEnable = VK_FALSE;

        VkPipelineColorBlendStateCreateInfo colorBlend{};
        colorBlend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlend.logicOpEnable = VK_FALSE;
        colorBlend.attachmentCount = 1;
        colorBlend.pAttachments = &colorBlendAttachment;

        VkDynamicState dynamicStates[] = {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR
        };

        VkPipelineDynamicStateCreateInfo dynamicState{};
        dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicState.dynamicStateCount = 2;
        dynamicState.pDynamicStates = dynamicStates;

        if (colorAttachmentFormat == VK_FORMAT_UNDEFINED)
        {
            throw std::runtime_error("Static mesh pipeline color attachment format is undefined.");
        }

        if (depthAttachmentFormat == VK_FORMAT_UNDEFINED)
        {
            throw std::runtime_error("Static mesh pipeline depth attachment format is undefined.");
        }

        VkPipelineRenderingCreateInfo renderingInfo{};
        renderingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
        renderingInfo.colorAttachmentCount = 1;
        renderingInfo.pColorAttachmentFormats = &colorAttachmentFormat;
        renderingInfo.depthAttachmentFormat = depthAttachmentFormat;
        renderingInfo.stencilAttachmentFormat = VK_FORMAT_UNDEFINED;

        VkGraphicsPipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.pNext = &renderingInfo;
        pipelineInfo.stageCount = 2;
        pipelineInfo.pStages = stages;
        pipelineInfo.pVertexInputState = &vertexInput;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState = &multisample;
        pipelineInfo.pDepthStencilState = &depthStencil;
        pipelineInfo.pColorBlendState = &colorBlend;
        pipelineInfo.pDynamicState = &dynamicState;
        pipelineInfo.layout = m_PipelineLayout;
        pipelineInfo.renderPass = VK_NULL_HANDLE;
        pipelineInfo.subpass = 0;

        if (vkCreateGraphicsPipelines(m_Device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_Pipeline) != VK_SUCCESS)
        {
            vkDestroyPipelineLayout(m_Device, m_PipelineLayout, nullptr);
            m_PipelineLayout = VK_NULL_HANDLE;
            throw std::runtime_error("Failed to create static mesh graphics pipeline.");
        }

        QUIET_CORE_INFO(
            "Created static mesh pipeline. colorFormat={} depthFormat={}",
            static_cast<int>(colorAttachmentFormat),
            static_cast<int>(depthAttachmentFormat)
        );
    }

    StaticMeshPipeline::~StaticMeshPipeline()
    {
        if (m_Pipeline != VK_NULL_HANDLE)
        {
            vkDestroyPipeline(m_Device, m_Pipeline, nullptr);
            m_Pipeline = VK_NULL_HANDLE;
        }

        if (m_PipelineLayout != VK_NULL_HANDLE)
        {
            vkDestroyPipelineLayout(m_Device, m_PipelineLayout, nullptr);
            m_PipelineLayout = VK_NULL_HANDLE;
        }
    }
}

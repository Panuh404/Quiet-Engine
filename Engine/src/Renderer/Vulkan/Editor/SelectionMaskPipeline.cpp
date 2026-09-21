#include "Renderer/Vulkan/Editor/SelectionMaskPipeline.h"

#include <cstdint>
#include <stdexcept>

#include "Quiet/Core/Log.h"
#include "Renderer/Vulkan/Context/LogicalDevice.h"
#include "Renderer/Vulkan/Mesh/StaticMesh.h"
#include "Renderer/Vulkan/Shader/ShaderModule.h"

namespace Quiet::Renderer::Vulkan
{
    SelectionMaskPipeline::SelectionMaskPipeline(const LogicalDevice &logicalDevice,
                                                 VkFormat colorAttachmentFormat,
                                                 VkFormat depthAttachmentFormat,
                                                 VkDescriptorSetLayout cameraSetLayout)
                                                     : m_Device(logicalDevice.GetHandle())
    {
        if (colorAttachmentFormat == VK_FORMAT_UNDEFINED)
            throw std::runtime_error("Selection mask pipeline color attachment format is undefined.");

        if (depthAttachmentFormat == VK_FORMAT_UNDEFINED)
            throw std::runtime_error("Selection mask pipeline depth attachment format is undefined.");

        if (cameraSetLayout == VK_NULL_HANDLE)
            throw std::runtime_error("Selection mask pipeline received a null camera descriptor layout.");

        ShaderModule vertexShader(m_Device, "Resources/Shaders/SelectionMask.vert.spv");
        ShaderModule fragmentShader(m_Device, "Resources/Shaders/SelectionMask.frag.spv");

        const VkPipelineShaderStageCreateInfo shaderStages[] =
        {
            vertexShader.MakeStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
            fragmentShader.MakeStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT)
        };

        VkPushConstantRange pushConstantRange{};
        pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        pushConstantRange.offset = 0;
        pushConstantRange.size = sizeof(SelectionMaskPushConstants);

        VkPipelineLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layoutInfo.setLayoutCount = 1;
        layoutInfo.pSetLayouts = &cameraSetLayout;
        layoutInfo.pushConstantRangeCount = 1;
        layoutInfo.pPushConstantRanges = &pushConstantRange;

        if (vkCreatePipelineLayout(m_Device, &layoutInfo, nullptr, &m_PipelineLayout) != VK_SUCCESS)
            throw std::runtime_error("Failed to create selection mask pipeline layout.");

        const VkVertexInputBindingDescription binding = GetStaticMeshVertexBindingDescription();
        const auto attributes = GetStaticMeshVertexAttributeDescriptions();

        VkPipelineVertexInputStateCreateInfo vertexInput{};
        vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInput.vertexBindingDescriptionCount = 1;
        vertexInput.pVertexBindingDescriptions = &binding;
        vertexInput.vertexAttributeDescriptionCount = 1;
        vertexInput.pVertexAttributeDescriptions = attributes.data();

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
        rasterizer.cullMode = VK_CULL_MODE_NONE;
        rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        rasterizer.depthBiasEnable = VK_FALSE;
        rasterizer.lineWidth = 1.0f;

        VkPipelineMultisampleStateCreateInfo multisample{};
        multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        VkPipelineDepthStencilStateCreateInfo depthStencil{};
        depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depthStencil.depthTestEnable = VK_TRUE;
        depthStencil.depthWriteEnable = VK_FALSE;
        depthStencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
        depthStencil.depthBoundsTestEnable = VK_FALSE;
        depthStencil.stencilTestEnable = VK_FALSE;

        VkPipelineColorBlendAttachmentState colorAttachment{};
        colorAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT;
        colorAttachment.blendEnable = VK_FALSE;

        VkPipelineColorBlendStateCreateInfo colorBlend{};
        colorBlend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlend.logicOpEnable = VK_FALSE;
        colorBlend.attachmentCount = 1;
        colorBlend.pAttachments = &colorAttachment;

        constexpr VkDynamicState dynamicStates[] =
        {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR
        };

        VkPipelineDynamicStateCreateInfo dynamicState{};
        dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicState.dynamicStateCount = 2;
        dynamicState.pDynamicStates = dynamicStates;

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
        pipelineInfo.pStages = shaderStages;
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
            throw std::runtime_error("Failed to create selection mask graphics pipeline.");
        }

        QUIET_CORE_INFO("Created selection mask pipeline. colorFormat={} depthFormat={}",
            static_cast<int>(colorAttachmentFormat),
            static_cast<int>(depthAttachmentFormat)
        );
    }

    SelectionMaskPipeline::~SelectionMaskPipeline()
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

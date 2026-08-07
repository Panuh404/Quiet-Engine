#include "Renderer/Vulkan/Editor/SelectionOutlineRenderSystem.h"

#include <stdexcept>

#include "Renderer/Vulkan/Editor/SelectionOutlinePipeline.h"

namespace Quiet::Renderer::Vulkan
{
    void SelectionOutlineRenderSystem::Record(const SelectionOutlineRenderContext& context) const
    {
        if (context.CommandBuffer == VK_NULL_HANDLE)
            throw std::runtime_error("SelectionOutlineRenderSystem received a null command buffer.");

        if (!context.Pipeline || !context.Pipeline->IsMaskImageReady())
            return;

        if (context.Extent.width == 0 || context.Extent.height == 0)
            return;

        vkCmdBindPipeline(context.CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, context.Pipeline->GetHandle());
        const VkDescriptorSet descriptorSet = context.Pipeline->GetDescriptorSet();

        vkCmdBindDescriptorSets(
            context.CommandBuffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            context.Pipeline->GetLayout(),
            0,
            1,
            &descriptorSet,
            0,
            nullptr
        );

        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = static_cast<float>(context.Extent.width);
        viewport.height = static_cast<float>(context.Extent.height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;

        vkCmdSetViewport(context.CommandBuffer, 0, 1, &viewport);

        VkRect2D scissor{};
        scissor.offset = { 0, 0 };
        scissor.extent = context.Extent;

        vkCmdSetScissor(context.CommandBuffer, 0, 1, &scissor);

        SelectionOutlinePushConstants push{};

        vkCmdPushConstants(
            context.CommandBuffer,
            context.Pipeline->GetLayout(),
            VK_SHADER_STAGE_FRAGMENT_BIT,
            0,
            sizeof(SelectionOutlinePushConstants),
            &push
        );

        vkCmdDraw(
            context.CommandBuffer,
            3,
            1,
            0,
            0
        );
    }
}
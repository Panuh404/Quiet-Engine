#include "Renderer/Vulkan/Text/TextRenderSystem.h"

#include <stdexcept>

#include "Renderer/Vulkan/RenderTypes.h"
#include "Renderer/Vulkan/Text/TextPipeline.h"

namespace Quiet::Renderer::Vulkan
{
    void TextRenderSystem::Record(const TextRenderContext& context) const
    {
        if (context.CommandBuffer == VK_NULL_HANDLE)
        {
            throw std::runtime_error("TextRenderSystem received a null command buffer.");
        }

        if (!context.Pipeline || context.Quads.empty() || context.Extent.width == 0 || context.Extent.height == 0)
        {
            return;
        }

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
            nullptr);

        VkViewport viewport{};
        viewport.width = static_cast<float>(context.Extent.width);
        viewport.height = static_cast<float>(context.Extent.height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(context.CommandBuffer, 0, 1, &viewport);

        VkRect2D scissor{};
        scissor.extent = context.Extent;
        vkCmdSetScissor(context.CommandBuffer, 0, 1, &scissor);

        TextPushConstants push{};
        push.Color = context.Color;
        push.Viewport = glm::vec4(static_cast<float>(context.Extent.width), static_cast<float>(context.Extent.height), 0.0f, 0.0f);

        for (const TextGlyphQuad& quad : context.Quads)
        {
            push.Rect = glm::vec4(
                quad.Min.x + context.Offset.x,
                quad.Min.y + context.Offset.y,
                quad.Max.x + context.Offset.x,
                quad.Max.y + context.Offset.y);

            push.UVRect = glm::vec4(quad.UVMin.x, quad.UVMin.y, quad.UVMax.x, quad.UVMax.y);

            vkCmdPushConstants(context.CommandBuffer, context.Pipeline->GetLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(TextPushConstants), &push);
            vkCmdDraw(context.CommandBuffer, 6, 1, 0, 0);
        }
    }
}
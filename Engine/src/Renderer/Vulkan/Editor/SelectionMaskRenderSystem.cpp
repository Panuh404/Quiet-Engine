#include "Renderer/Vulkan/Editor/SelectionMaskRenderSystem.h"

#include <stdexcept>

#include "Renderer/Vulkan/Editor/SelectionMaskPipeline.h"
#include "Renderer/Vulkan/Frame/CameraResources.h"
#include "Renderer/Vulkan/Mesh/StaticMesh.h"

#include "Scene/Components/StaticMeshComponent.h"
#include "Scene/Components/TransformComponent.h"

namespace Quiet::Renderer::Vulkan
{
    void SelectionMaskRenderSystem::Record(
        const SelectionMaskRenderContext& context) const
    {
        if (context.CommandBuffer == VK_NULL_HANDLE)
            throw std::runtime_error("SelectionMaskRenderSystem received a null command buffer.");

        if (!context.Pipeline || !context.Camera || !context.Registry)
            throw std::runtime_error("SelectionMaskRenderSystem received an invalid render context.");

        if (context.SelectedEntity == entt::null || context.Extent.width == 0 || context.Extent.height == 0)
            return;

        if (!context.Registry->valid(context.SelectedEntity))
            return;

        const Scene::StaticMeshComponent* staticMesh = context.Registry->try_get<Scene::StaticMeshComponent>(context.SelectedEntity);
        const Scene::TransformComponent* transform = context.Registry->try_get<Scene::TransformComponent>(context.SelectedEntity);

        if (!staticMesh || !transform || !staticMesh->IsValid() || !staticMesh->Visible)
            return;

        const auto& mesh = staticMesh->Mesh;
        if (!mesh || mesh->GetIndexCount() == 0)
            return;

        vkCmdBindPipeline(context.CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, context.Pipeline->GetHandle());
        const VkDescriptorSet cameraDescriptor = context.Camera->GetDescriptorSet(context.ImageIndex);

        vkCmdBindDescriptorSets(
            context.CommandBuffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            context.Pipeline->GetLayout(),
            0,
            1,
            &cameraDescriptor,
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

        const VkBuffer vertexBuffers[] = { mesh->GetVertexBuffer() };
        constexpr VkDeviceSize offsets[] = { 0 };

        vkCmdBindVertexBuffers(
            context.CommandBuffer,
            0,
            1,
            vertexBuffers,
            offsets
        );

        vkCmdBindIndexBuffer(
            context.CommandBuffer,
            mesh->GetIndexBuffer(),
            0,
            VK_INDEX_TYPE_UINT32
        );

        SelectionMaskPushConstants push{};

        push.Model = transform->WorldTransform.ToMatrix();

        vkCmdPushConstants(
            context.CommandBuffer,
            context.Pipeline->GetLayout(),
            VK_SHADER_STAGE_VERTEX_BIT,
            0,
            sizeof(SelectionMaskPushConstants),
            &push
        );

        vkCmdDrawIndexed(
            context.CommandBuffer,
            mesh->GetIndexCount(),
            1,
            0,
            0,
            0
        );
    }
}
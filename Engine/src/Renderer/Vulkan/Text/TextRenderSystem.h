#pragma once

#include <optional>
#include <span>

#include <glm/vec2.hpp>
#include <glm/vec4.hpp>
#include <vulkan/vulkan.h>

#include "Quiet/Renderer/TextLayout.h"

namespace Quiet::Renderer::Vulkan
{
    class TextPipeline;

    struct TextRenderContext
    {
        VkCommandBuffer CommandBuffer = VK_NULL_HANDLE;
        const TextPipeline* Pipeline = nullptr;

        VkExtent2D Extent{};
        std::span<const TextGlyphQuad> Quads{};

        glm::vec2 Offset{0.0f};
        glm::vec4 Color{1.0f};

        /**
         * @brief Optional caller-resolved target scissor.
         * @note Must lie within Extent and satisfy Vulkan scissor limits.
         */
        std::optional<VkRect2D> Scissor;
    };

    /**
     * @brief Records glyph draws without owning GPU resources.
     * @ingroup quiet_renderer
     */
    class TextRenderSystem
    {
    public:
        /**
         * @brief Records one six-vertex draw per supplied glyph.
         * @param context Inputs borrowed only during this call.
         * @pre CommandBuffer is recording inside a color-only rendering scope
         *      compatible with the supplied pipeline.
         * @note Sets a positive-height viewport and the supplied scissor,
         *       defaulting to the complete target.
         * @note Does not restore previously bound graphics state.
         * @note Missing pipeline, empty geometry, or zero extent skips drawing.
         */
        void Record(const TextRenderContext& context) const;
    };
}
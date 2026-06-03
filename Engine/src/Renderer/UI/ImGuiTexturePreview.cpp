#include "Renderer/UI/ImGuiTexturePreview.h"

#include <algorithm>
#include <memory>
#include <string>
#include <unordered_map>

#include <backends/imgui_impl_vulkan.h>
#include <vulkan/vulkan.h>

#include "Renderer/Vulkan/Texture/Texture2D.h"
#include "Renderer/Vulkan/Texture/TextureLibrary.h"

namespace
{
    struct TexturePreviewCacheEntry
    {
        VkImageView ImageView = VK_NULL_HANDLE;
        VkSampler Sampler = VK_NULL_HANDLE;
        VkDescriptorSet DescriptorSet = VK_NULL_HANDLE;
    };

    std::unordered_map<std::string, TexturePreviewCacheEntry>& GetTexturePreviewCache()
    {
        static std::unordered_map<std::string, TexturePreviewCacheEntry> cache;
        return cache;
    }

    void DrawPreviewFrameOutline(ImDrawList* drawList,
                                 const ImVec2& min,
                                 const ImVec2& max,
                                 const ImU32 color,
                                 const float thickness)
    {
        drawList->AddLine(min, ImVec2(max.x, min.y), color, thickness);
        drawList->AddLine(ImVec2(max.x, min.y), max, color, thickness);

        drawList->AddLine(max, ImVec2(min.x, max.y), color, thickness);
        drawList->AddLine(ImVec2(min.x, max.y), min, color, thickness);
    }

    void DrawPreviewFrameFilled(ImDrawList* drawList,
                                const ImVec2& min,
                                const ImVec2& max,
                                const ImU32 color)
    {
        drawList->AddQuadFilled(min, ImVec2(max.x, min.y), max, ImVec2(min.x, max.y), color);
    }
}

namespace Quiet::Renderer::UI
{
    ImTextureID GetOrCreateTexturePreviewID(const std::string_view textureAssetID, const Vulkan::TextureLibrary& textureLibrary)
    {
        const std::string textureID(textureAssetID);
        const std::shared_ptr<Vulkan::Texture2D> texture = textureLibrary.TryGet(textureID);

        if (!texture) return ImTextureID{};
        const VkImageView imageView = texture->GetImageView();
        const VkSampler sampler = texture->GetSampler();

        if (imageView == VK_NULL_HANDLE || sampler == VK_NULL_HANDLE) return ImTextureID{};
        auto& cache = GetTexturePreviewCache();
        const auto it = cache.find(textureID);

        if (it != cache.end())
        {
            const TexturePreviewCacheEntry& entry = it->second;

            if (entry.ImageView == imageView &&
                entry.Sampler == sampler &&
                entry.DescriptorSet != VK_NULL_HANDLE)
                return reinterpret_cast<ImTextureID>(entry.DescriptorSet);

            if (entry.DescriptorSet != VK_NULL_HANDLE)
                ImGui_ImplVulkan_RemoveTexture(entry.DescriptorSet);
        }

        const VkDescriptorSet descriptorSet = ImGui_ImplVulkan_AddTexture(
            sampler,
            imageView,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        );

        if (descriptorSet == VK_NULL_HANDLE)
            return ImTextureID{};

        cache[textureID] = TexturePreviewCacheEntry{
            imageView,
            sampler,
            descriptorSet
        };

        return reinterpret_cast<ImTextureID>(descriptorSet);
    }

    ImVec2 CalculateTexturePreviewSize(const std::uint32_t width,
                                       const std::uint32_t height,
                                       const float maxSide)
    {
        if (width == 0 || height == 0) return {maxSide, maxSide};

        const auto maxDimension = static_cast<float>(std::max(width, height));
        const float scale = maxSide / maxDimension;

        return {
            std::max(1.0f, static_cast<float>(width) * scale),
            std::max(1.0f, static_cast<float>(height) * scale)
        };
    }

    void DrawTexturePreview(const std::string_view textureAssetID,
                            const Vulkan::TextureLibrary& textureLibrary,
                            const Vulkan::TextureStats& textureStats,
                            const float uiScale,
                            const float maxPreviewSideBase)
    {
        const ImTextureID previewID = GetOrCreateTexturePreviewID(textureAssetID, textureLibrary);

        if (!previewID)
        {
            ImGui::TextDisabled("Texture preview is unavailable.");
            return;
        }

        const float maxPreviewSide = maxPreviewSideBase * uiScale;
        const float framePadding = 10.0f * uiScale;

        const ImVec2 previewSize = CalculateTexturePreviewSize(
            textureStats.Width,
            textureStats.Height,
            maxPreviewSide
        );

        const ImVec2 frameSize(
            previewSize.x + framePadding * 2.0f,
            previewSize.y + framePadding * 2.0f
        );

        const float availableWidth = ImGui::GetContentRegionAvail().x;
        if (availableWidth > frameSize.x)
        {
            ImGui::SetCursorPosX(
                ImGui::GetCursorPosX() + (availableWidth - frameSize.x) * 0.5f
            );
        }

        const ImVec2 frameMin = ImGui::GetCursorScreenPos();
        const ImVec2 frameMax(
            frameMin.x + frameSize.x,
            frameMin.y + frameSize.y
        );

        const ImVec2 imageMin(
            frameMin.x + framePadding,
            frameMin.y + framePadding
        );

        const ImVec2 imageMax(
            imageMin.x + previewSize.x,
            imageMin.y + previewSize.y
        );

        ImDrawList* drawList = ImGui::GetWindowDrawList();

        DrawPreviewFrameFilled(
            drawList,
            frameMin,
            frameMax,
            IM_COL32(24, 27, 33, 255)
        );

        DrawPreviewFrameOutline(
            drawList,
            frameMin,
            frameMax,
            IM_COL32(72, 82, 96, 255),
            1.0f * uiScale
        );

        // Texture pixels are flipped during import to match mesh UVs.
        // Flip preview UVs so the user sees the source texture upright.
        drawList->AddImage(
            previewID,
            imageMin,
            imageMax,
            ImVec2(0.0f, 1.0f),
            ImVec2(1.0f, 0.0f),
            IM_COL32_WHITE
        );

        ImGui::Dummy(frameSize);
        ImGui::TextDisabled("Preview uses the imported GPU texture.");
    }

    void ClearTexturePreviewCache(bool releaseDescriptors)
    {
        auto& cache = GetTexturePreviewCache();

        if (releaseDescriptors)
        {
            for (const auto& [textureID, entry] : cache)
            {
                (void)textureID;
                if (entry.DescriptorSet != VK_NULL_HANDLE)
                    ImGui_ImplVulkan_RemoveTexture(entry.DescriptorSet);
            }
        }
        cache.clear();
    }
}

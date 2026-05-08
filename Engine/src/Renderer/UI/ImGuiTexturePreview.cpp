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
                            const float uiScale)
    {
        const ImTextureID previewID = GetOrCreateTexturePreviewID(textureAssetID, textureLibrary);
        if (!previewID)
        {
            ImGui::TextDisabled("Texture preview is unavailable.");
            return;
        }

        const float maxPreviewSide = 240.0f * uiScale;
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
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (availableWidth - frameSize.x) * 0.5f);
        }

        const std::string childID = "TexturePreviewFrame##" + std::string(textureAssetID);

        if (ImGui::BeginChild(
                childID.c_str(),
                frameSize,
                true,
                ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
        {
            ImGui::SetCursorPos(ImVec2(framePadding, framePadding));

            // Texture pixels are flipped during import to match mesh UVs.
            // Flip preview UVs so the user sees the source texture upright.
            ImGui::Image(
                previewID,
                previewSize,
                ImVec2(0.0f, 1.0f),
                ImVec2(1.0f, 0.0f)
            );
        }

        ImGui::EndChild();
        ImGui::TextDisabled("Preview uses the imported GPU texture.");
    }
}

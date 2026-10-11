#pragma once

#include <filesystem>

#include "Quiet/Project/RuntimeManifest.h"

namespace Quiet::Audio
{
    class AudioLibrary;
}

namespace Quiet::Renderer::Vulkan
{
    class GraphicsCommands;
    class LogicalDevice;
    class MeshLibrary;
    class PhysicalDevice;
    class TextureLibrary;
}

namespace Quiet::Project
{
    /**
     * @brief Loads packaged assets into externally owned resource libraries.
     * @ingroup quiet_assets
     *
     * Receives an already validated manifest and borrows runtime dependencies.
     * Batch failure does not roll back previously loaded resources.
     */
    class RuntimeAssetLoader
    {
    public:
        RuntimeAssetLoader(
            Renderer::Vulkan::MeshLibrary & meshLibrary,
            Renderer::Vulkan::TextureLibrary& textureLibrary,
            Audio::AudioLibrary& audioLibrary,
            const Renderer::Vulkan::PhysicalDevice& physicalDevice,
            const Renderer::Vulkan::LogicalDevice& logicalDevice,
            const Renderer::Vulkan::GraphicsCommands& graphicsCommands);

        /**
         * @brief Reads packaged QMesh records and creates or replaces mesh resources.
         * @return True when no selected record reports failure.
         * @note Reported failures do not stop remaining mesh records.
         * @note Uncaught exceptions propagate; no batch rollback is provided.
         */
        bool LoadMeshAssets(const std::filesystem::path& packageRoot, const RuntimeManifest& manifest);

        /**
         * @brief Reads packaged QTexture records and creates or replaces textures.
         * @return True when no selected record reports failure.
         * @note Reported failures do not stop remaining texture records.
         * @note Uncaught exceptions propagate; no batch rollback is provided.
         */
        bool LoadTextureAssets(const std::filesystem::path& packageRoot, const RuntimeManifest& manifest);

        /**
         * @brief Loads packaged audio files with existing-entry replacement enabled.
         * @return True when no selected record reports failure.
         * @note Reported failures do not stop remaining audio records.
         * @note Does not use a QAudio reader.
         */
        bool LoadAudioAssets(const std::filesystem::path& packageRoot, const RuntimeManifest& manifest);
    private:
        bool LoadMeshAsset(const std::filesystem::path& packageRoot, const RuntimeAssetRecord& record);
        bool LoadTextureAsset(const std::filesystem::path& packageRoot, const RuntimeAssetRecord& record);
        bool LoadAudioAsset(const std::filesystem::path& packageRoot, const RuntimeAssetRecord& record);

    private:
        Renderer::Vulkan::MeshLibrary& m_MeshLibrary;
        Renderer::Vulkan::TextureLibrary& m_TextureLibrary;
        Audio::AudioLibrary& m_AudioLibrary;

        const Renderer::Vulkan::PhysicalDevice& m_PhysicalDevice;
        const Renderer::Vulkan::LogicalDevice& m_LogicalDevice;
        const Renderer::Vulkan::GraphicsCommands& m_GraphicsCommands;
    };
}
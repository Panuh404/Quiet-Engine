#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "Quiet/Project/AssetRegistry.h"

namespace Quiet::Project
{
    class ProjectContext;
}

namespace Quiet::Audio
{
    class AudioLibrary;
}

namespace Quiet::Renderer::Vulkan
{
    class MeshLibrary;
    class TextureLibrary;
    class PhysicalDevice;
    class LogicalDevice;
    class GraphicsCommands;
}

namespace Quiet::Project
{
    enum class AssetReloadStage : std::uint8_t
    {
        Idle,
        Meshes,
        Textures,
        Audio,
        Complete
    };

    struct AssetReloadProgress
    {
        AssetReloadStage Stage = AssetReloadStage::Idle;
        std::size_t Completed = 0;
        std::size_t Total = 0;
        std::string CurrentAssetID;
        bool HadFailures = false;
    };

    /**
     * @brief Loads project assets into externally owned resource libraries.
     * @ingroup quiet_assets
     *
     * Borrows project context, libraries, and renderer resources.
     * Loading can partially succeed and does not automatically relink Scenes.
     */
    class ProjectAssetLoader
    {
    public:
        ProjectAssetLoader(ProjectContext& projectContext,
                           Renderer::Vulkan::MeshLibrary& meshLibrary,
                           Renderer::Vulkan::TextureLibrary& textureLibrary,
                           Audio::AudioLibrary& audioLibrary,
                           const Renderer::Vulkan::PhysicalDevice& physicalDevice,
                           const Renderer::Vulkan::LogicalDevice& logicalDevice,
                           const Renderer::Vulkan::GraphicsCommands& graphicsCommands);

        void BindRendererResources(const Renderer::Vulkan::PhysicalDevice& physicalDevice,
                                   const Renderer::Vulkan::LogicalDevice& logicalDevice,
                                   const Renderer::Vulkan::GraphicsCommands& graphicsCommands);

        bool ReloadMeshAssets(bool forceReimport = false);
        bool ReloadBuiltInMeshAssets();

        bool ReloadTextureAssets(bool forceReimport = false);
        bool ReloadAudioAssets(bool forceReimport = false);

        bool ReloadAssets(bool forceReimport = false);
        bool ReloadRuntimeSupportAssets();
        bool ReloadAssetByID(std::string_view assetID, bool forceReimport = true);

        #pragma region Incremental Reloading
        /**
         * @brief Resets incremental progress, clears audio, and prepares mesh loading.
         * @param forceReimport Requests replacement of existing external resources.
         * @note Mesh and texture libraries are not cleared.
         * @note Empty stages can complete immediately.
         */
        void BeginIncrementalReload(bool forceReimport = false);

        /**
         * @brief Attempts one record or advances a completed loading stage.
         * @note Failed results still advance progress and set HadFailures.
         * @note Executes synchronously; uncaught exceptions interrupt advancement.
         */
        void AdvanceIncrementalReload();

        /**
         * @brief Reports whether the sequence reached Complete.
         * @note Completion does not imply that all loads succeeded.
         */
        [[nodiscard]] bool IsIncrementalReloadComplete() const
        {
            return m_IncrementalReloadProgress.Stage == AssetReloadStage::Complete;
        }

        /**
         * @brief Returns current-stage counts and sequence failure information.
         * @note Completed counts attempted records, including reported failures.
         * @note Counts reset at stage transitions.
         */
        [[nodiscard]] const AssetReloadProgress& GetIncrementalReloadProgress() const
        {
            return m_IncrementalReloadProgress;
        }
        #pragma endregion

    private:
        bool LoadBuiltInMesh(const AssetRecord& record);
        bool LoadExternalMesh(const AssetRecord& record, bool forceReimport);
        bool LoadExternalTexture(const AssetRecord& record, bool forceReimport);
        bool LoadExternalAudio(const AssetRecord& record, bool forceReimport);

        void PrepareIncrementalMeshAssets();
        void PrepareIncrementalTextureAssets();
        void PrepareIncrementalAudioAssets();

    private:
        ProjectContext& m_ProjectContext;
        Renderer::Vulkan::MeshLibrary& m_MeshLibrary;
        Renderer::Vulkan::TextureLibrary& m_TextureLibrary;
        Audio::AudioLibrary& m_AudioLibrary;
        const Renderer::Vulkan::PhysicalDevice* m_PhysicalDevice = nullptr;
        const Renderer::Vulkan::LogicalDevice* m_LogicalDevice = nullptr;
        const Renderer::Vulkan::GraphicsCommands* m_GraphicsCommands = nullptr;

        std::vector<AssetRecord> m_IncrementalReloadAssets;
        std::size_t m_IncrementalReloadIndex = 0;
        bool m_IncrementalForceReimport = false;
        AssetReloadProgress m_IncrementalReloadProgress{};
    };
}
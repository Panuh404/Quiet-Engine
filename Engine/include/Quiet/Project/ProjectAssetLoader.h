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

    class ProjectAssetLoader
    {
    public:
        ProjectAssetLoader(ProjectContext& projectContext,
                           Renderer::Vulkan::MeshLibrary& meshLibrary,
                           Renderer::Vulkan::TextureLibrary& textureLibrary,
                           const Renderer::Vulkan::PhysicalDevice& physicalDevice,
                           const Renderer::Vulkan::LogicalDevice& logicalDevice,
                           const Renderer::Vulkan::GraphicsCommands& graphicsCommands);

        void BindRendererResources(const Renderer::Vulkan::PhysicalDevice& physicalDevice,
                                   const Renderer::Vulkan::LogicalDevice& logicalDevice,
                                   const Renderer::Vulkan::GraphicsCommands& graphicsCommands);

        bool ReloadMeshAssets(bool forceReimport = false);
        bool ReloadTextureAssets(bool forceReimport = false);
        bool ReloadAssets(bool forceReimport = false);
        bool ReloadAssetByID(std::string_view assetID, bool forceReimport = true);

        void BeginIncrementalReload(bool forceReimport = false);
        void AdvanceIncrementalReload();

        [[nodiscard]] bool IsIncrementalReloadComplete() const { return m_IncrementalReloadProgress.Stage == AssetReloadStage::Complete; }
        [[nodiscard]] const AssetReloadProgress& GetIncrementalReloadProgress() const { return m_IncrementalReloadProgress; }

    private:
        bool LoadBuiltInMesh(const AssetRecord& record);
        bool LoadExternalMesh(const AssetRecord& record, bool forceReimport);
        bool LoadExternalTexture(const AssetRecord& record, bool forceReimport);

        void PrepareIncrementalMeshAssets();
        void PrepareIncrementalTextureAssets();

    private:
        ProjectContext& m_ProjectContext;
        Renderer::Vulkan::MeshLibrary& m_MeshLibrary;
        Renderer::Vulkan::TextureLibrary& m_TextureLibrary;
        const Renderer::Vulkan::PhysicalDevice* m_PhysicalDevice = nullptr;
        const Renderer::Vulkan::LogicalDevice* m_LogicalDevice = nullptr;
        const Renderer::Vulkan::GraphicsCommands* m_GraphicsCommands = nullptr;

        std::vector<AssetRecord> m_IncrementalReloadAssets;
        std::size_t m_IncrementalReloadIndex = 0;
        bool m_IncrementalForceReimport = false;
        AssetReloadProgress m_IncrementalReloadProgress{};
    };
}
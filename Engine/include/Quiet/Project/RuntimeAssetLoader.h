#pragma once

#include <filesystem>

#include "Quiet/Project/RuntimeManifest.h"

namespace Quiet::Renderer::Vulkan
{
    class GraphicsCommands;
    class LogicalDevice;
    class MeshLibrary;
    class PhysicalDevice;
}

namespace Quiet::Project
{
    class RuntimeAssetLoader
    {
    public:
        RuntimeAssetLoader(
            Renderer::Vulkan::MeshLibrary & meshLibrary,
            const Renderer::Vulkan::PhysicalDevice& physicalDevice,
            const Renderer::Vulkan::LogicalDevice& logicalDevice,
            const Renderer::Vulkan::GraphicsCommands& graphicsCommands);

        bool LoadMeshAssets(const std::filesystem::path& packageRoot, const RuntimeManifest& manifest);

    private:
        bool LoadMeshAsset(const std::filesystem::path& packageRoot, const RuntimeAssetRecord& record);

    private:
        Renderer::Vulkan::MeshLibrary& m_MeshLibrary;

        const Renderer::Vulkan::PhysicalDevice& m_PhysicalDevice;
        const Renderer::Vulkan::LogicalDevice& m_LogicalDevice;
        const Renderer::Vulkan::GraphicsCommands& m_GraphicsCommands;
    };
}
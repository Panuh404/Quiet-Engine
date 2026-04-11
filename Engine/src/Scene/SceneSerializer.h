#pragma once

#include <filesystem>

namespace Quiet::Scene
{
    class Scene;
}

namespace Quiet::Renderer::Vulkan
{
    class MeshLibrary;
}

namespace Quiet::Scene
{
    class SceneSerializer
    {
    public:
        static bool SaveToFile(const Scene& scene,
                               const std::filesystem::path& path);

        static bool LoadFromFile(Scene& scene,
                                 const Renderer::Vulkan::MeshLibrary& meshLibrary,
                                 const std::filesystem::path& path);
    };
}
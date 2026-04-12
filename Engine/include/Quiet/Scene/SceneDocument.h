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
    class SceneDocument
    {
    public:
        static void CreateDefault(Scene& scene,
                                  const Renderer::Vulkan::MeshLibrary& meshLibrary);

        static bool Save(const Scene& scene,
                         const std::filesystem::path& path);

        static bool Load(Scene& scene,
                         const Renderer::Vulkan::MeshLibrary& meshLibrary,
                         const std::filesystem::path& path);
    };
}
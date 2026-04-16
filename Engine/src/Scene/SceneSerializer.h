#pragma once

#include <filesystem>
#include <nlohmann/json_fwd.hpp>

namespace Quiet::Scene
{
    struct CameraComponent;
    class Actor;
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

    private:
        static nlohmann::json SerializeCamera(const CameraComponent& camera);
        static void DeserializeCamera(const nlohmann::json& json, Actor& actor);
    };
}
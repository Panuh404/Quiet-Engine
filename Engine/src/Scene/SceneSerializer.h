#pragma once

#include <filesystem>
#include <nlohmann/json_fwd.hpp>

namespace Quiet::Scene
{
    struct CameraComponent;
    struct MaterialComponent;

    class Actor;
    class Scene;
}

namespace Quiet::Renderer::Vulkan
{
    class MeshLibrary;
}

namespace Quiet::Scene
{
    /**
     * @brief Saves supported Scene data and reconstructs Scenes from JSON files.
     * @ingroup quiet_scene
     *
     * Runtime worlds and backend execution state are not persisted.
     */
    class SceneSerializer
    {
    public:
        /**
         * @brief Writes supported current Scene data as formatted JSON.
         * @param scene Scene whose current component values are recorded.
         * @param path Destination file; parent directories are not created.
         * @return False if the file cannot be opened; true after issuing the write.
         * @note Existing file contents are replaced directly.
         * @note Final stream status is not checked; true does not guarantee
         *       a complete successful write.
         * @note JSON construction and serialization exceptions propagate.
         */
        static bool SaveToFile(const Scene& scene, const std::filesystem::path& path);

        /**
         * @brief Reconstructs an inactive Scene from JSON and loaded meshes.
         * @param scene Destination cleared after parsing and the actors-array check.
         * @param meshLibrary Already-loaded resources used for mesh ID resolution.
         * @param path Source Scene file.
         * @return False if opening fails or the actors-array check fails;
         *         true when reconstruction reaches completion.
         * @note Missing meshes are warned about and their components omitted.
         * @note True does not guarantee that every file entry was retained.
         * @note Parsing and reconstruction exceptions propagate.
         * @note Failures after clearing can leave a partially reconstructed Scene.
         */
        static bool LoadFromFile(Scene& scene, const Renderer::Vulkan::MeshLibrary& meshLibrary, const std::filesystem::path& path);

    private:
        static nlohmann::json SerializeCamera(const CameraComponent& camera);
        static void DeserializeCamera(const nlohmann::json& json, Actor& actor);

        static nlohmann::json SerializeMaterial(const MaterialComponent& material);
        static MaterialComponent DeserializeMaterial(const nlohmann::json& materialJson);
    };
}
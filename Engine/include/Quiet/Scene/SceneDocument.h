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
    /**
     * @brief Exposes default Scene construction and file persistence operations.
     * @ingroup quiet_scene
     *
     * Save and Load forward directly to SceneSerializer without adding
     * fallback, exception handling, or rollback.
     */
    class SceneDocument
    {
    public:
        /**
         * @brief Clears the Scene and creates its default Cube and primary Camera.
         * @param scene Destination whose existing state is cleared.
         * @param meshLibrary Library supplying the required "Cube" mesh resource.
         * @note Missing Cube lookup throws after the destination has been cleared.
         */
        static void CreateDefault(Scene& scene, const Renderer::Vulkan::MeshLibrary& meshLibrary);

        /**
         * @brief Forwards Scene saving to SceneSerializer.
         * @param scene Scene whose current data is recorded.
         * @param path Destination file.
         * @return False if opening fails; true after the serializer issues its write.
         * @note Final stream status is not checked by the serializer.
         * @note Serializer exceptions propagate.
         */
        static bool Save(const Scene& scene, const std::filesystem::path& path);

        /**
         * @brief Forwards Scene reconstruction to SceneSerializer.
         * @param scene Destination reconstructed in place.
         * @param meshLibrary Already-loaded resources used for mesh resolution.
         * @param path Source Scene file.
         * @return The serializer's result; true can include skipped entries.
         * @note Serializer exceptions propagate and can leave partial state.
         */
        static bool Load(Scene& scene, const Renderer::Vulkan::MeshLibrary& meshLibrary, const std::filesystem::path& path);
    };
}
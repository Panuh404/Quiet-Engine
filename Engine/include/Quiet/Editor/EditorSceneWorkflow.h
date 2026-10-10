#pragma once
#include <filesystem>

namespace Quiet::Project
{
    class ProjectContext;
}

namespace Quiet::Scene
{
    class Scene;
}

namespace Quiet::Renderer::Vulkan
{
    class MeshLibrary;
}

namespace Quiet::Editor
{
    class EditorController;

    /**
     * @brief Coordinates editable Scene file operations and session resets.
     * @ingroup quiet_editor
     *
     * Borrows the Scene, project context, mesh library, and Editor controller.
     * These dependencies must outlive the workflow.
     */
    class EditorSceneWorkflow
    {
    public:
        EditorSceneWorkflow(Scene::Scene& scene,
                            Project::ProjectContext& projectContext,
                            Renderer::Vulkan::MeshLibrary& meshLibrary,
                            EditorController& editorController);

        /**
         * @brief Checks Edit mode and logs rejection using the action label.
         * @return True only in Edit mode.
         */
        [[nodiscard]] bool EnsureEditMode(const char* action) const;

        /**
         * @brief Resets selection, piloting, history, and script tracking state.
         * @note Establishes the current Scene as clean without writing a file.
         * @note Does not independently check Edit mode.
         */
        void ResetSceneSession();

        /**
         * @brief Sets the display name from the active Scene filename.
         */
        void UpdateSceneDisplayName();

        /**
         * @brief Creates a default Scene in Edit mode and resets its session.
         * @note Resets the active path to the project's startup Scene path.
         * @note Does not save a file; exceptions propagate.
         */
        void NewScene();

        /**
         * @brief Saves the editable Scene to its active path.
         * @return False on mode rejection, directory failure, or serializer failure.
         * @note Success updates the clean baseline without clearing history.
         * @note Inherits serializer write-status limitations and exceptions.
         */
        bool SaveScene();

        /**
         * @brief Loads the active Scene file in Edit mode.
         * @return The load result, or false when Edit mode is required.
         * @note Success resets the session; no default Scene fallback is added.
         * @note Failed loading or exceptions can leave partial Scene state.
         */
        bool LoadScene();

        /**
         * @brief Changes the active path and attempts to load that Scene.
         * @param relativeScenePath Path resolved by ProjectContext.
         * @return True on successful loading; false on rejection or load failure.
         * @note False restores the previous path, not the previous Scene content.
         * @note Exceptions propagate without restoring the previous path.
         */
        bool OpenSceneInProject(const std::filesystem::path& relativeScenePath);

    private:
        Scene::Scene& m_Scene;
        Project::ProjectContext& m_ProjectContext;
        Renderer::Vulkan::MeshLibrary& m_MeshLibrary;
        EditorController& m_EditorController;
    };

}
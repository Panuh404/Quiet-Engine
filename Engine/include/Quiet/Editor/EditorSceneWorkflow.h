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

    class EditorSceneWorkflow
    {
    public:
        EditorSceneWorkflow(Scene::Scene& scene,
                            Project::ProjectContext& projectContext,
                            Renderer::Vulkan::MeshLibrary& meshLibrary,
                            EditorController& editorController);

        [[nodiscard]] bool EnsureEditMode(const char* action) const;

        void ResetSceneSession();
        void UpdateSceneDisplayName();

        void NewScene();
        bool SaveScene();
        bool LoadScene();
        bool OpenSceneInProject(const std::filesystem::path& relativeScenePath);

    private:
        Scene::Scene& m_Scene;
        Project::ProjectContext& m_ProjectContext;
        Renderer::Vulkan::MeshLibrary& m_MeshLibrary;
        EditorController& m_EditorController;
    };

}
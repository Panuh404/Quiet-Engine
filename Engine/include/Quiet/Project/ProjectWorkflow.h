#pragma once
#include <filesystem>

namespace Quiet::Scene
{
    class Scene;
}

namespace Quiet::Project
{
    class ProjectAssetLoader;
}

namespace Quiet::Renderer::Vulkan
{
    class MeshLibrary;
}

namespace Quiet::Project
{
    struct ProjectRegistry;
    class ProjectContext;

    class ProjectWorkflow
    {
    public:
        ProjectWorkflow(ProjectContext& projectContext,
                        ProjectRegistry& projectRegistry,
                        ProjectAssetLoader& projectAssetLoader,
                        Scene::Scene& scene,
                        Renderer::Vulkan::MeshLibrary& meshLibrary);

        bool InitializeProjectRegistry();
        bool InitializeProject(const std::filesystem::path& projectFilePath);
        bool InitializeLastOpenedOrDefaultProject();

        bool BeginStagedOpenProject(const std::filesystem::path& projectFilePath);
        void FinishStagedOpenProject();

        [[nodiscard]] std::filesystem::path GetInitialProjectFilePath() const;

        bool OpenOrCreateProject(const std::filesystem::path& projectFilePath);
        bool CreateProject(const std::string& projectName);
        bool SetStartupScene(const std::filesystem::path& relativeScenePath);

    private:
        bool LoadOrCreateRegistry();
        bool SaveRegistry();
        void TouchRecentProject();
        void LoadOrCreateStartupScene();
        [[nodiscard]] std::filesystem::path ResolveInitialProjectFilePath() const;

    private:
        ProjectContext& m_ProjectContext;
        ProjectRegistry& m_ProjectRegistry;
        ProjectAssetLoader& m_ProjectAssetLoader;
        Scene::Scene& m_Scene;
        Renderer::Vulkan::MeshLibrary& m_MeshLibrary;
    };
}
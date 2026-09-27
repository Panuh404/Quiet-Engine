#include "Quiet/Project/ProjectWorkflow.h"

#include <cctype>

#include "Quiet/Core/Log.h"
#include "Quiet/Project/ProjectAssetLoader.h"
#include "Quiet/Project/ProjectContext.h"
#include "Quiet/Project/ProjectRegistry.h"
#include "Quiet/Scene/SceneDocument.h"

namespace
{
    bool LoadOrCreateProjectRegistry(Quiet::Project::ProjectRegistry& registry)
    {
        const std::filesystem::path registryPath =
            Quiet::Project::ProjectRegistryIO::GetDefaultRegistryFilePath();

        if (!std::filesystem::exists(registryPath))
        {
            registry = Quiet::Project::ProjectRegistryIO::MakeDefault();
            std::filesystem::create_directories(registry.ProjectsRoot);
            return Quiet::Project::ProjectRegistryIO::SaveToFile(registryPath, registry);
        }

        if (!Quiet::Project::ProjectRegistryIO::LoadFromFile(registryPath, registry))
            return false;

        std::filesystem::create_directories(registry.ProjectsRoot);
        return true;
    }

    void LoadOrCreateStartupScene(Quiet::Scene::Scene& scene,
                                  Quiet::Renderer::Vulkan::MeshLibrary& meshLibrary,
                                  const std::filesystem::path& scenePath)
    {
        if (Quiet::Scene::SceneDocument::Load(scene, meshLibrary, scenePath))
        {
            QUIET_CORE_INFO("Loaded startup scene from '{}'.", scenePath.string());
            return;
        }

        Quiet::Scene::SceneDocument::CreateDefault(scene, meshLibrary);
        QUIET_CORE_WARN(
            "Failed to load startup scene from '{}'. Created default scene instead.",
            scenePath.string()
        );
    }

    std::string TrimCopy(std::string value)
    {
        const auto isSpace = [](unsigned char c) { return std::isspace(c) != 0; };
        while (!value.empty() && isSpace(static_cast<unsigned char>(value.front())))
        {
            value.erase(value.begin());
        }

        while (!value.empty() && isSpace(static_cast<unsigned char>(value.back())))
        {
            value.pop_back();
        }
        return value;
    }

    bool IsValidProjectName(const std::string& name)
    {
        if (name.empty()) return false;
        if (name == "." || name == "..") return false;

        static constexpr const char* invalidChars = "\\/:*?\"<>|";
        return name.find_first_of(invalidChars) == std::string::npos;

    }
}

namespace Quiet::Project
{
    ProjectWorkflow::ProjectWorkflow(ProjectContext &projectContext,
                                     ProjectRegistry &projectRegistry,
                                     ProjectAssetLoader &projectAssetLoader,
                                     Scene::Scene &scene,
                                     Renderer::Vulkan::MeshLibrary &meshLibrary)
                                         : m_ProjectContext(projectContext)
                                         , m_ProjectRegistry(projectRegistry)
                                         , m_ProjectAssetLoader(projectAssetLoader)
                                         , m_Scene(scene)
                                         , m_MeshLibrary(meshLibrary) {}

    bool ProjectWorkflow::InitializeProjectRegistry()
    {
        if (!LoadOrCreateRegistry())
        {
            QUIET_CORE_ERROR("Failed to load or create project registry.");
            return false;
        }
        return true;
    }

    std::filesystem::path ProjectWorkflow::GetInitialProjectFilePath() const
    {
        return ResolveInitialProjectFilePath();
    }

    bool ProjectWorkflow::InitializeProject(const std::filesystem::path& projectFilePath)
    {
        if (!InitializeProjectRegistry())
        {
            return false;
        }

        if (projectFilePath.empty())
        {
            QUIET_CORE_ERROR("Cannot initialize an empty explicit project path.");
            return false;
        }

        const std::filesystem::path normalizedProjectPath = std::filesystem::absolute(projectFilePath).lexically_normal();
        if (normalizedProjectPath.extension() != ".quiet")
        {
            QUIET_CORE_ERROR("Explicit project path is not a Quiet project: '{}'.", normalizedProjectPath.string());
            return false;
        }

        std::error_code ec;
        if (!std::filesystem::exists(normalizedProjectPath, ec) || ec)
        {
            QUIET_CORE_ERROR("Explicit project does not exist: '{}'.", normalizedProjectPath.string());
            return false;
        }

        return OpenOrCreateProject(normalizedProjectPath);
    }

    bool ProjectWorkflow::InitializeLastOpenedOrDefaultProject()
    {
        if (!InitializeProjectRegistry()) return false;
        return OpenOrCreateProjectInternal(ResolveInitialProjectFilePath(), true);
    }

    bool ProjectWorkflow::InitializeLastOpenedOrDefaultProjectAssetsOnly()
    {
        if (!InitializeProjectRegistry()) return false;
        return OpenOrCreateProjectInternal(ResolveInitialProjectFilePath(), false);
    }

    bool ProjectWorkflow::BeginStagedOpenProject(const std::filesystem::path &projectFilePath)
    {
        if (!LoadOrCreateProject(m_ProjectContext, projectFilePath))
        {
            QUIET_CORE_ERROR("Failed to prepare project '{}'.", projectFilePath.string());
            return false;
        }

        TouchRecentProject();
        if (!SaveRegistry())
        {
            QUIET_CORE_WARN("Failed to save project registry.");
        }

        m_ProjectAssetLoader.BeginIncrementalReload();

        QUIET_CORE_INFO("Prepared staged project load for '{}'.", m_ProjectContext.GetProjectFilePath().string());
        return true;
    }

    void ProjectWorkflow::FinishStagedOpenProject()
    {
        const AssetReloadProgress& progress = m_ProjectAssetLoader.GetIncrementalReloadProgress();
        if (progress.HadFailures)
        {
            QUIET_CORE_WARN("Project '{}' has assets that are registered but not fully loadable yet.", m_ProjectContext.GetProjectFilePath().string());
        }

        LoadOrCreateStartupScene();
        QUIET_CORE_INFO("Opened project '{}'.", m_ProjectContext.GetProjectFilePath().string());
    }

    bool ProjectWorkflow::OpenOrCreateProject(const std::filesystem::path &projectFilePath)
    {
        return OpenOrCreateProjectInternal(projectFilePath, true);
    }

    bool ProjectWorkflow::CreateProject(const std::string &projectName)
    {
        const std::string trimmedName = TrimCopy(projectName);
        if (!IsValidProjectName(trimmedName))
        {
            QUIET_CORE_WARN("Invalid project name '{}'.", projectName);
            return false;
        }

        const std::filesystem::path projectFolder = m_ProjectRegistry.ProjectsRoot / trimmedName;
        const std::filesystem::path projectFilePath = projectFolder / (trimmedName + ".quiet");
        return OpenOrCreateProject(projectFilePath);
    }

    bool ProjectWorkflow::SetStartupScene(const std::filesystem::path &relativeScenePath)
    {
        if (relativeScenePath.empty()) return false;
        m_ProjectContext.SetStartupScene(relativeScenePath);
        if (!m_ProjectContext.SaveDescriptor())
        {
            QUIET_CORE_ERROR(
                "Failed to save project descriptor '{}'.",
                m_ProjectContext.GetProjectFilePath().string()
            );
            return false;
        }

        QUIET_CORE_INFO("Set startup scene to '{}'.", relativeScenePath.generic_string());
        return true;
    }

    bool ProjectWorkflow::OpenOrCreateProjectInternal(const std::filesystem::path &projectFilePath, bool loadStartupScene)
    {
        if (!LoadOrCreateProject(m_ProjectContext, projectFilePath))
        {
            QUIET_CORE_ERROR("Failed to open project '{}'.", projectFilePath.string());
            return false;
        }

        TouchRecentProject();
        if (!SaveRegistry())
        {
            QUIET_CORE_WARN("Failed to save project registry.");
        }

        const bool assetsLoaded = loadStartupScene
            ? m_ProjectAssetLoader.ReloadAssets()
            : m_ProjectAssetLoader.ReloadRuntimeSupportAssets();

        if (!assetsLoaded)
        {
            QUIET_CORE_WARN("Project '{}' has assets that are registered but not fully loadable yet.", m_ProjectContext.GetProjectFilePath().string());
        }

        if (loadStartupScene)
        {
            LoadOrCreateStartupScene();
            QUIET_CORE_INFO("Opened project '{}'.", m_ProjectContext.GetProjectFilePath().string());
        }
        else
        {
            QUIET_CORE_INFO("Loaded project assets from '{}' without loading the source startup scene.", m_ProjectContext.GetProjectFilePath().string());
        }

        return true;
    }

    bool ProjectWorkflow::LoadOrCreateRegistry()
    {
        return LoadOrCreateProjectRegistry(m_ProjectRegistry);
    }

    bool ProjectWorkflow::SaveRegistry()
    {
        return ProjectRegistryIO::SaveToFile(
            ProjectRegistryIO::GetDefaultRegistryFilePath(),
            m_ProjectRegistry
        );
    }

    void ProjectWorkflow::TouchRecentProject()
    {
        ProjectRegistryIO::TouchRecentProject(
            m_ProjectRegistry,
            m_ProjectContext.GetProjectFilePath()
        );
    }

    void ProjectWorkflow::LoadOrCreateStartupScene()
    {
        ::LoadOrCreateStartupScene(
            m_Scene,
            m_MeshLibrary,
            m_ProjectContext.GetStartupScenePath()
        );
    }

    std::filesystem::path ProjectWorkflow::ResolveInitialProjectFilePath() const
    {
        std::filesystem::path projectFilePath = m_ProjectRegistry.LastOpenedProjectFile;
        if (!projectFilePath.empty()) return projectFilePath;

        const std::string defaultProjectName = "SandboxProject";
        const std::filesystem::path projectFolder =
            m_ProjectRegistry.ProjectsRoot / defaultProjectName;

        return projectFolder / (defaultProjectName + ".quiet");
    }
}

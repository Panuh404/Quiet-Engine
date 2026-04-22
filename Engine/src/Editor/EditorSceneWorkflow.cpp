#include "Quiet/Editor/EditorSceneWorkflow.h"

#include "Quiet/Core/Log.h"
#include "Quiet/Editor/EditorController.h"
#include "Quiet/Project/ProjectContext.h"
#include "Quiet/Scene/SceneDocument.h"

namespace Quiet::Editor
{
    EditorSceneWorkflow::EditorSceneWorkflow(Scene::Scene &scene,
                                             Project::ProjectContext &projectContext,
                                             Renderer::Vulkan::MeshLibrary &meshLibrary,
                                             EditorController &editorController)
                                                 : m_Scene(scene)
                                                 , m_ProjectContext(projectContext)
                                                 , m_MeshLibrary(meshLibrary)
                                                 , m_EditorController(editorController){}

    bool EditorSceneWorkflow::EnsureEditMode(const char *action) const
    {
        const auto& editorState = m_EditorController.GetState();
        if (editorState.Scene.Mode == SceneMode::Edit) return true;

        QUIET_CORE_WARN("Cannot {} while runtime mode is active.", action);
        return false;
    }

    void EditorSceneWorkflow::ResetSceneSession()
    {
        auto& editorState = m_EditorController.GetState();
        UpdateSceneDisplayName();

        editorState.Selection.PilotedCameraActorID.reset();
        m_EditorController.ResetSelection();
        m_EditorController.ResetSceneHistory(m_Scene);
    }

    void EditorSceneWorkflow::UpdateSceneDisplayName()
    {
        auto& editorState = m_EditorController.GetState();
        editorState.Scene.DisplayName = m_ProjectContext.GetActiveScenePath().filename().string();
    }

    void EditorSceneWorkflow::NewScene()
    {
        if (!EnsureEditMode("create a new scene")) return;

        Scene::SceneDocument::CreateDefault(m_Scene, m_MeshLibrary);
        m_ProjectContext.ResetActiveScenePath();
        ResetSceneSession();

        QUIET_CORE_INFO("Created new scene.");
    }

    bool EditorSceneWorkflow::SaveScene()
    {
        if (!EnsureEditMode("save scene")) return false;
        if (!m_ProjectContext.EnsureDirectoriesExist())
        {
            QUIET_CORE_ERROR("Failed to create project scene directories.");
            return false;
        }

        const std::filesystem::path scenePath = m_ProjectContext.GetActiveScenePath();
        if (!Scene::SceneDocument::Save(m_Scene, scenePath))
        {
            QUIET_CORE_ERROR("Failed to save scene to '{}'.", scenePath.string());
            return false;
        }

        UpdateSceneDisplayName();
        m_EditorController.MarkSceneSaved(m_Scene);

        QUIET_CORE_INFO("Saved scene to '{}'.", scenePath.string());
        return true;
    }

    bool EditorSceneWorkflow::LoadScene()
    {
        if (!EnsureEditMode("load scene")) return false;

        const std::filesystem::path scenePath = m_ProjectContext.GetActiveScenePath();
        if (!Scene::SceneDocument::Load(m_Scene, m_MeshLibrary, scenePath))
        {
            QUIET_CORE_WARN("Failed to load scene from '{}'.", scenePath.string());
            return false;
        }

        ResetSceneSession();

        QUIET_CORE_INFO("Loaded scene from '{}'.", scenePath.string());
        return true;
    }

    bool EditorSceneWorkflow::OpenSceneInProject(const std::filesystem::path &relativeScenePath)
    {
        if (!EnsureEditMode("open scene")) return false;

        const std::filesystem::path previousScenePath = m_ProjectContext.GetActiveScenePath();
        m_ProjectContext.SetActiveScenePath(relativeScenePath);

        if (LoadScene()) return true;

        m_ProjectContext.SetActiveScenePath(previousScenePath);
        return false;
    }
}

#pragma once

#include <filesystem>
#include <string>
#include <unordered_map>

#include "EditorSceneHistory.h"
#include "EditorState.h"

namespace Quiet::Project
{
    class ProjectContext;
}

namespace Quiet::Input
{
    class Input;
}

namespace Quiet::Scene
{
    class FreeCamera;
    class Scene;
}

namespace Quiet::Renderer::Vulkan
{
    class VulkanRenderer;
}

namespace Quiet::Editor
{
    class EditorController
    {
    public:
        EditorController() = default;
        ~EditorController() = default;

        [[nodiscard]] EditorState& GetState() { return m_State; }
        [[nodiscard]] const EditorState& GetState() const { return m_State; }

        void ResetSelection();

        void ResetSceneHistory(const Scene::Scene& scene);
        void MarkSceneSaved(const Scene::Scene& scene);
        void BeginSceneMutation(const Scene::Scene& scene);
        bool EndSceneMutation(const Scene::Scene& scene);
        bool Undo(Scene::Scene& scene);
        bool Redo(Scene::Scene& scene);

        void SynchronizeScriptSources(Scene::Scene& scene, const Project::ProjectContext& projectContext);

        void HandleShortcuts(const Input::Input& input, bool mouseCaptureEnabled);
        void FocusSelectedActor(Scene::Scene& scene,
                                Scene::FreeCamera& camera);

        void TrySelectActorFromViewport(const Input::Input& input,
                                        Scene::Scene& scene,
                                        bool mouseCaptureEnabled);

        void UpdateHoveredActorFromViewport(const Scene::Scene& scene,
                                            const Scene::FreeCamera& camera,
                                            const Renderer::Vulkan::VulkanRenderer& renderer,
                                            bool mouseCaptureEnabled);

        void BuildSelectedActorGizmo(Scene::Scene& scene,
                                     Scene::FreeCamera& camera,
                                     const Renderer::Vulkan::VulkanRenderer& renderer,
                                     bool mouseCaptureEnabled);

        void DrawViewportDebugIcons(const Scene::Scene& scene,
                                    const Scene::FreeCamera& camera,
                                    const Renderer::Vulkan::VulkanRenderer& renderer) const;

    private:
        EditorState m_State{};
        EditorSceneHistory m_History{};
        bool m_WasUsingGizmo = false;

        std::unordered_map<std::string, std::filesystem::file_time_type> m_ScriptSourceWriteTimes{};
    };
}
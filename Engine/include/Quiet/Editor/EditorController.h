#pragma once

#include <optional>

#include "EditorSceneHistory.h"
#include "EditorState.h"
#include "Quiet/Core/UUID.h"
#include "Quiet/Math/Geometry.h"

namespace Quiet::Input
{
    class Input;
}

namespace Quiet::Scene
{
    class FreeCamera;
}

namespace Quiet::Math
{
    struct Ray;
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

        void HandleShortcuts(const Input::Input& input, bool mouseCaptureEnabled);
        void FocusSelectedActor(Scene::Scene& scene,
                                Scene::FreeCamera& camera);

        void TrySelectActorFromViewport(const Input::Input& input,
                                        Scene::Scene& scene,
                                        const Scene::FreeCamera& camera,
                                        const Renderer::Vulkan::VulkanRenderer& renderer,
                                        bool mouseCaptureEnabled);

        void BuildSelectedActorGizmo(Scene::Scene& scene,
                                     Scene::FreeCamera& camera,
                                     const Renderer::Vulkan::VulkanRenderer& renderer,
                                     bool mouseCaptureEnabled);

    private:
        [[nodiscard]] bool BuildViewportPickRay(const Input::Input& input,
                                                const Scene::FreeCamera& camera,
                                                const Renderer::Vulkan::VulkanRenderer& renderer,
                                                Math::Ray& outRay) const;

    private:
        EditorState m_State{};
        EditorSceneHistory m_History{};
        bool m_WasUsingGizmo = false;
    };
}
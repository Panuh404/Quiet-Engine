#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include "AppSpec.h"
#include "EditorState.h"

#pragma region Forward declarations

namespace Quiet::Platform
{
    class Window;
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

namespace Quiet::Math
{
    struct Ray;
}

#pragma endregion

namespace Quiet::Core
{
    class Application
    {
    public:
        explicit Application(AppSpec spec);
        ~Application();

        int Run();

    private:
        void ToggleMouseCapture();
        void SetMouseCaptureEnabled(bool enabled);

        void HandleEditorShortcuts();
        void TrySelectActorFromViewport();
        void FocusSelectedActor();
        [[nodiscard]] bool BuildViewportPickRay(Math::Ray& outRay) const;
        void BuildSelectedActorGizmo();

        void NewScene();
        void SaveScene();
        void LoadScene();

    private:
        AppSpec m_Spec;
        bool m_Running = true;
        bool m_MouseCaptureEnabled = true;

        std::unique_ptr<Platform::Window> m_Window;
        std::unique_ptr<Input::Input> m_Input;
        std::unique_ptr<Scene::FreeCamera> m_FreeCamera;
        std::unique_ptr<Scene::Scene> m_Scene;
        std::unique_ptr<Renderer::Vulkan::VulkanRenderer> m_Renderer;

        EditorState m_EditorState{};
    };
}
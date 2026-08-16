#pragma once

namespace Quiet::Editor
{
    class EditorCommands;
    struct EditorState;
}

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
    class TextureLibrary;
}

namespace Quiet::Renderer::UI::Inspector
{
    struct InspectorContext
    {
        Scene::Scene& Scene;
        Editor::EditorCommands& Commands;
        Editor::EditorState& EditorState;

        const Project::ProjectContext& ProjectContext;
        const Vulkan::MeshLibrary& MeshLibrary;
        const Vulkan::TextureLibrary& TextureLibrary;

        float UIScale = 1.0f;
    };
}
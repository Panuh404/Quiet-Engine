#include "Renderer/UI/ImGuiLayer.h"

#include "Renderer/UI/Inspector/ImGuiInspectorContext.h"
#include "Renderer/UI/Inspector/ImGuiInspectorPanel.h"
#include "Renderer/UI/Inspector/ImGuiInspectorState.h"

namespace Quiet::Renderer::UI
{
    void ImGuiLayer::BuildInspector(
        Scene::Scene& scene,
        Editor::EditorCommands& editorCommands,
        Editor::EditorState& editorState,
        const Project::ProjectContext& projectContext,
        const Vulkan::MeshLibrary& meshLibrary,
        const Vulkan::TextureLibrary& textureLibrary)
    {
        static Inspector::InspectorState inspectorState{};

        Inspector::InspectorContext inspectorContext{
            scene,
            editorCommands,
            editorState,
            projectContext,
            meshLibrary,
            textureLibrary,
            GetUIScale()
        };

        Inspector::DrawInspectorPanel(
            inspectorContext,
            inspectorState
        );
    }
}
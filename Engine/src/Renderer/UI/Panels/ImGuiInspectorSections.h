#pragma once

#include "Quiet/Editor/EditorCommands.h"
#include "Quiet/Editor/EditorState.h"
#include "Scene/Actor.h"

namespace Quiet::Scene
{
    class Scene;
}

namespace Quiet::Renderer::Vulkan
{
    class MeshLibrary;
}

namespace Quiet::Renderer::UI
{
    struct TrackedInspectorInteraction
    {
        Scene::Scene& SceneRef;
        Editor::EditorCommands& Commands;
        bool Enabled = false;

        void Begin() const;
        void End() const;
    };

    void DrawActorDetailsSection(Scene::Scene& scene,
                                 Scene::Actor& actor,
                                 Editor::EditorCommands& commands,
                                 const TrackedInspectorInteraction& tracked);

    void DrawStaticMeshSection(Scene::Scene& scene,
                               Scene::Actor& actor,
                               Editor::EditorCommands& commands,
                               const Vulkan::MeshLibrary& meshLibrary);

    void DrawRotatorSection(Scene::Scene& scene,
                            Scene::Actor& actor,
                            Editor::EditorCommands& commands,
                            const TrackedInspectorInteraction& tracked);

    void DrawCameraSection(Scene::Scene& scene,
                           Scene::Actor& actor,
                           Editor::EditorCommands& commands,
                           Editor::EditorState& editorState,
                           const TrackedInspectorInteraction& tracked,
                           bool isEditMode);
}
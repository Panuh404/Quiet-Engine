#pragma once

#include "ImGuiInspectorContext.h"
#include "ImGuiInspectorState.h"
#include "Quiet/Editor/EditorCommands.h"
#include "Quiet/Editor/EditorState.h"
#include "Scene/Actor.h"

namespace Quiet::Scene
{
    class Scene;
}

namespace Quiet::Project
{
    class ProjectContext;
}

namespace Quiet::Renderer::Vulkan
{
    class MeshLibrary;
    class TextureLibrary;
}

namespace Quiet::Renderer::UI::Inspector
{
    struct TrackedInspectorInteraction
    {
        Scene::Scene& SceneRef;
        Editor::EditorCommands& Commands;
        bool Enabled = false;

        void Begin() const;
        void End() const;
    };

    void DrawActorDetailsSection(InspectorContext& context, InspectorState& state, Scene::Actor& actor, const TrackedInspectorInteraction& tracked);
    void DrawAddComponentMenu(InspectorContext& context, Scene::Actor& actor);
    void DrawStaticMeshSection(InspectorContext& context, InspectorState& state, Scene::Actor& actor);
    void DrawMaterialSection(InspectorContext& context, InspectorState& state, Scene::Actor& actor, const TrackedInspectorInteraction& tracked);
    void DrawDirectionalLightSection(InspectorContext& context, InspectorState& state, Scene::Actor& actor, const TrackedInspectorInteraction& tracked);
    void DrawRotatorSection(InspectorContext& context, InspectorState& state, Scene::Actor& actor, const TrackedInspectorInteraction& tracked);
    void DrawTextSection(InspectorContext& context, InspectorState& state, Scene::Actor& actor, const TrackedInspectorInteraction& tracked);
    void DrawUIRectSection(InspectorContext& context, InspectorState& state, Scene::Actor& actor, const TrackedInspectorInteraction& tracked);
    void DrawCameraSection(InspectorContext& context, InspectorState& state, Scene::Actor& actor, const TrackedInspectorInteraction& tracked, bool isEditMode);
    void DrawAudioSourceSection(InspectorContext& context, InspectorState& state, Scene::Actor& actor, const TrackedInspectorInteraction& tracked);
    void DrawAudioListenerSection(InspectorContext& context, InspectorState& state, Scene::Actor& actor);
    void DrawScriptSection(InspectorContext& context, InspectorState& state, Scene::Actor& actor, const TrackedInspectorInteraction& tracked);
    void DrawRigidBodySection(InspectorContext& context, InspectorState& state, Scene::Actor& actor, const TrackedInspectorInteraction& tracked);
    void DrawBoxColliderSection(InspectorContext& context, InspectorState& state, Scene::Actor& actor, const TrackedInspectorInteraction& tracked);
    void DrawSphereColliderSection(InspectorContext& context, InspectorState& state, Scene::Actor& actor, const TrackedInspectorInteraction& tracked);
    void DrawCapsuleColliderSection(InspectorContext& context, InspectorState& state, Scene::Actor& actor, const TrackedInspectorInteraction& tracked);
}
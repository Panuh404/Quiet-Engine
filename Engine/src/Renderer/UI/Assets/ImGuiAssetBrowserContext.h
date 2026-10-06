#pragma once

#include "Quiet/Project/ProjectContext.h"
#include "Quiet/Editor/EditorState.h"
#include "Scene/Scene.h"
#include "Renderer/UI/ImGuiFonts.h"
#include "Renderer/Vulkan/Mesh/MeshLibrary.h"
#include "Renderer/Vulkan/Texture/TextureLibrary.h"

namespace Quiet::Audio
{
    class AudioSubsystem;
}

namespace Quiet::Renderer::UI::Assets
{
    struct AssetBrowserContext
    {
        Project::ProjectContext& ProjectContext;
        Editor::EditorState& EditorState;
        Scene::Scene& Scene;

        const Vulkan::MeshLibrary& MeshLibrary;
        const Vulkan::TextureLibrary& TextureLibrary;
        Audio::AudioSubsystem& AudioSubsystem;

        const EditorFonts& Fonts;

        float UIScale = 1.0f;
    };
}
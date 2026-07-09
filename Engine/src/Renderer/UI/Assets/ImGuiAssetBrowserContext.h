#pragma once

#include "Quiet/Project/ProjectContext.h"
#include "Quiet/Editor/EditorState.h"
#include "Scene/Scene.h"
#include "Renderer/Vulkan/Mesh/MeshLibrary.h"
#include "Renderer/Vulkan/Texture/TextureLibrary.h"

namespace Quiet::Renderer::UI::Assets
{
    struct AssetBrowserContext
    {
        Project::ProjectContext& ProjectContext;
        Editor::EditorState& EditorState;
        Scene::Scene& Scene;
        const Vulkan::MeshLibrary& MeshLibrary;
        const Vulkan::TextureLibrary& TextureLibrary;
        float UIScale = 1.0f;
    };
}
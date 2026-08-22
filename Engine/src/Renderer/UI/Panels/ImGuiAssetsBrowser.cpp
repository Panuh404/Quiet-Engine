#include "Renderer/UI/ImGuiLayer.h"
#include "Renderer/UI/Assets/ImGuiAssetBrowserContext.h"
#include "Renderer/UI/Assets/ImGuiAssetBrowserPanel.h"
#include "Renderer/UI/Assets/ImGuiAssetBrowserTypes.h"

namespace Quiet::Renderer::UI
{
    void ImGuiLayer::BuildAssetsPanel(
        Project::ProjectContext& projectContext,
        Editor::EditorState& editorState,
        Scene::Scene& scene,
        const Vulkan::MeshLibrary& meshLibrary,
        const Vulkan::TextureLibrary& textureLibrary,
        const Audio::AudioLibrary& audioLibrary)
    {
        static Assets::AssetBrowserState assetBrowserState{};
        Assets::AssetBrowserContext assetBrowserContext{
            projectContext,
            editorState,
            scene,
            meshLibrary,
            textureLibrary,
            audioLibrary,
            GetUIScale()
        };

        Assets::DrawAssetBrowserPanel(
            assetBrowserContext,
            assetBrowserState
        );
    }
}

#pragma once

#include "Renderer/UI/Assets/ImGuiAssetBrowserContext.h"
#include "Renderer/UI/Assets/ImGuiAssetBrowserTypes.h"

namespace Quiet::Renderer::UI::Assets
{
    void DrawAssetBrowserPanel(AssetBrowserContext& context, AssetBrowserState& state);
}
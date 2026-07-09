#pragma once

#include <string>

#include "Quiet/Project/ProjectContext.h"

namespace Quiet::Renderer::UI::Assets
{
    void DrawAssetBrowserSourcesPanel(const Project::ProjectContext& projectContext, std::string& currentAssetFolder, float width);
}
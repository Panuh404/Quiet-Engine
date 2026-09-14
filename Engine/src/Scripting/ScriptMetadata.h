#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "Scene/Components/ScriptComponent.h"

namespace Quiet::Project
{
    class ProjectContext;
}

namespace Quiet::Scripting
{
    bool DiscoverScriptProperties(
        const Project::ProjectContext& projectContext,
        std::string_view scriptAssetID,
        std::vector<Scene::ScriptProperty>& outProperties,
        std::string& outError);
}
#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <filesystem>

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

    bool GetScriptSourceWriteTime(
        const Project::ProjectContext& projectContext,
        std::string_view scriptAssetID,
        std::filesystem::file_time_type& outWriteTime,
        std::string& outError);
}
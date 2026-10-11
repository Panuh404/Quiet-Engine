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
    bool ResolveScriptSourcePath(
        const Project::ProjectContext& projectContext,
        std::string_view scriptAssetID,
        std::filesystem::path& outSourcePath,
        std::string& outError);

    /**
     * @brief Executes a script in a temporary Lua state to discover properties.
     * @ingroup quiet_scripting
     * @return False on source resolution, script execution, or declaration failure.
     * @note Installs discovery Expose functions without runtime Actor/Scene/Input.
     * @note Requires unique, nonempty names and finite numeric declaration defaults.
     * @note Clears discovered properties on execution or declaration failure.
     */
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
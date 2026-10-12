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
    /**
     * @brief Resolves a registered Script asset to an available regular file.
     * @ingroup quiet_scripting
     * @param projectContext Project supplying the asset registry and root directory.
     * @param scriptAssetID Registered Script asset ID to resolve.
     * @param outSourcePath Resolved, lexically normalized path.
     * @param outError Failure description; cleared before resolution.
     * @return True when the asset type, path, and file checks succeed.
     * @note Relative paths are resolved against the project root.
     * @note Absolute paths are accepted; project-root containment is not enforced.
     * @note On failure, outSourcePath may contain the attempted path.
     */
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

    /**
     * @brief Resolves a Script asset and reads its source modification timestamp.
     * @ingroup quiet_scripting
     * @param projectContext Project supplying the asset registry and root directory.
     * @param scriptAssetID Registered Script asset ID whose source is inspected.
     * @param outWriteTime Timestamp on success; default-initialized on failure.
     * @param outError Failure description; cleared before inspection.
     * @return False when source resolution or timestamp inspection fails.
     */
    bool GetScriptSourceWriteTime(
        const Project::ProjectContext& projectContext,
        std::string_view scriptAssetID,
        std::filesystem::file_time_type& outWriteTime,
        std::string& outError);
}
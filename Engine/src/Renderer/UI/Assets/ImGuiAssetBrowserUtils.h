#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

#include "Quiet/Project/AssetRegistry.h"
#include "Quiet/Project/ProjectContext.h"
#include "Renderer/UI/Assets/ImGuiAssetBrowserTypes.h"
#include "Renderer/Vulkan/Mesh/MeshLibrary.h"
#include "Renderer/Vulkan/Texture/TextureLibrary.h"
#include "Scene/Scene.h"

namespace Quiet::Renderer::UI::Assets
{
    // Text / string helpers
    std::string ToLowerCopy(std::string value);
    bool ContainsCaseInsensitive(const std::string& text, const std::string& query);

    template <std::size_t Size>
    void CopyToBuffer(std::array<char, Size>& buffer, const std::string& value)
    {
        std::snprintf(buffer.data(), buffer.size(), "%s", value.c_str());
    }

    std::string ExtractFileNameNoExt(const std::string& path);

    // Registry search / filtering
    bool MatchesTypeFilter(const Project::AssetRecord& record, AssetBrowserTypeFilter filter);
    bool MatchesAssetSearch(const Project::AssetRecord& record, const std::string& query);
    bool MatchesAssetFolder(const Project::ProjectContext& projectContext, const Project::AssetRecord& record, const std::string& currentAssetFolder);

    std::vector<const Project::AssetRecord*> BuildAssetFolderList(
        const Project::ProjectContext& projectContext,
        const Project::AssetRegistry& registry,
        const std::string& currentAssetFolder
    );

    std::vector<const Project::AssetRecord*> BuildVisibleAssetList(
        const std::vector<const Project::AssetRecord*>& folderAssets,
        AssetBrowserTypeFilter typeFilter,
        const std::string& searchQuery
    );

    std::size_t CountAssetByType(const Project::AssetRegistry& registry, Project::AssetType type);

    // File/path helpers
    std::string NormalizeProjectRelativePathString(const std::filesystem::path& path);
    std::string GetAssetsRootRelativePath(const Project::ProjectContext& projectContext);
    bool IsAssetFolderInsideAssetsRoot(const std::string& candidateFolder, const std::string& assetsRoot);

    std::filesystem::path ResolveAssetFolderPath(const Project::ProjectContext& projectContext, const std::string& assetFolder);
    std::string MakeProjectRelativeAssetFolderPath(const Project::ProjectContext& projectContext, const std::filesystem::path& absoluteFolderPath);
    std::string GetAssetBrowserFolderForRecord(const Project::ProjectContext& projectContext, const Project::AssetRecord& record);
    void ValidateCurrentAssetFolder(const Project::ProjectContext& projectContext, std::string& currentAssetFolder);
    std::string GetParentAssetFolder(const Project::ProjectContext& projectContext, const std::string& currentAssetFolder);
    std::vector<std::string> BuildAssetFolderBreadcrumbs(const Project::ProjectContext& projectContext, const std::string& currentAssetFolder);

    std::filesystem::path ResolveDisplayPath(const Project::ProjectContext& projectContext, const std::string& assetPath);
    bool DoesAssetPathExist(const Project::ProjectContext& projectContext, const std::string& assetPath);

    // Asset display/status helpers
    std::string MakeStatusLabel(const Project::AssetRecord& record, const Vulkan::MeshLibrary& meshLibrary, const Vulkan::TextureLibrary& textureLibrary);
    std::string MakeAssetDisplayName(const Project::AssetRecord& record);

    bool IsAssetReadyStatus(const std::string& status);
    bool IsAssetMissingStatus(const std::string& status);

    std::string MakeAssetBrowserStateLabel(
        const Project::AssetRecord& record,
        const std::string& status,
        bool runtimeModeActive,
        std::uint32_t referenceCount
    );

    // Scene usage helpers
    std::uint32_t CountMeshReferences(const Scene::Scene& scene, const std::string& meshID);
}
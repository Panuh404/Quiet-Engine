#include "Renderer/UI/Assets/ImGuiAssetBrowserUtils.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <system_error>

namespace Quiet::Renderer::UI::Assets
{
    std::string ToLowerCopy(std::string value)
    {
        std::transform(
            value.begin(),
            value.end(),
            value.begin(),
            [](const unsigned char c)
            {
                return static_cast<char>(std::tolower(c));
            }
        );
        return value;
    }

    bool ContainsCaseInsensitive(const std::string &text, const std::string &query)
    {
        if (query.empty()) return true;

        const std::string lowerText = ToLowerCopy(text);
        const std::string lowerQuery = ToLowerCopy(query);

        return lowerText.find(lowerQuery) != std::string::npos;
    }

    bool MatchesTypeFilter(const Project::AssetRecord &record, AssetBrowserTypeFilter filter)
    {
        switch (filter)
        {
            case AssetBrowserTypeFilter::All:       return true;
            case AssetBrowserTypeFilter::Meshes:    return record.Type == Project::AssetType::Mesh;
            case AssetBrowserTypeFilter::Textures:  return record.Type == Project::AssetType::Texture;
            default:                                return true;
        }
    }

    bool MatchesAssetSearch(const Project::AssetRecord &record, const std::string &query)
    {
        if (query.empty()) return true;
        if (ContainsCaseInsensitive(record.ID, query)) return true;
        if (ContainsCaseInsensitive(record.Path, query)) return true;

        const std::string typeLabel = std::string(Project::ToString(record.Type));
        return ContainsCaseInsensitive(typeLabel, query);
    }

    bool MatchesAssetFolder(
        const Project::ProjectContext& projectContext,
        const Project::AssetRecord& record,
        const std::string& currentAssetFolder)
    {
        const std::string assetsRoot = GetAssetsRootRelativePath(projectContext);
        const std::string normalizedFolder = currentAssetFolder.empty()
            ? assetsRoot
            : NormalizeProjectRelativePathString(currentAssetFolder);

        if (!IsAssetFolderInsideAssetsRoot(normalizedFolder, assetsRoot)) return false;
        return GetAssetBrowserFolderForRecord(projectContext, record) == normalizedFolder;
    }

    std::vector<const Project::AssetRecord*> BuildAssetFolderList(
        const Project::ProjectContext& projectContext,
        const Project::AssetRegistry& registry,
        const std::string& currentAssetFolder)
    {
        std::vector<const Project::AssetRecord*> folderAssets;
        folderAssets.reserve(registry.Records.size());

        for (const Project::AssetRecord& record : registry.Records)
        {
            if (!record.IsValid()) continue;
            if (!MatchesAssetFolder(projectContext, record, currentAssetFolder)) continue;
            folderAssets.push_back(&record);
        }
        return folderAssets;
    }

    std::vector<const Project::AssetRecord*> BuildVisibleAssetList(
        const std::vector<const Project::AssetRecord*>& folderAssets,
        const AssetBrowserTypeFilter typeFilter,
        const std::string& searchQuery)
    {
        std::vector<const Project::AssetRecord*> visibleAssets;
        visibleAssets.reserve(folderAssets.size());

        for (const Project::AssetRecord* record : folderAssets)
        {
            if (!record || !record->IsValid()) continue;
            if (!MatchesTypeFilter(*record, typeFilter)) continue;
            if (!MatchesAssetSearch(*record, searchQuery)) continue;
            visibleAssets.push_back(record);
        }
        return visibleAssets;
    }

    std::size_t CountAssetByType(const Project::AssetRegistry& registry, const Project::AssetType type)
    {
        std::size_t count = 0;
        for (const Project::AssetRecord& record : registry.Records)
        {
            if (record.IsValid() && record.Type == type) ++count;
        }
        return count;
    }

    std::string ExtractFileNameNoExt(const std::string& path)
    {
        return std::filesystem::path(path).stem().string();
    }

    std::string NormalizeProjectRelativePathString(const std::filesystem::path& path)
    {
        const std::string normalized = path.lexically_normal().generic_string();
        return normalized == "." ? std::string{} : normalized;
    }

    std::string GetAssetsRootRelativePath(const Project::ProjectContext& projectContext)
    {
        const std::string assetsDirectory = NormalizeProjectRelativePathString(projectContext.GetProjectDescriptor().AssetsDirectory);
        return assetsDirectory.empty() ? "Assets" : assetsDirectory;
    }

    bool IsAssetFolderInsideAssetsRoot(const std::string& candidateFolder, const std::string& assetsRoot)
    {
        if (candidateFolder == assetsRoot) return true;
        return candidateFolder.rfind(assetsRoot + "/", 0) == 0;
    }

    std::filesystem::path ResolveAssetFolderPath(const Project::ProjectContext& projectContext, const std::string& assetFolder)
    {
        if (assetFolder.empty()) return projectContext.GetAssetsPath();
        std::filesystem::path folderPath = assetFolder;
        if (folderPath.is_relative()) folderPath = projectContext.GetRootPath() / folderPath;

        return folderPath.lexically_normal();
    }

    std::string MakeProjectRelativeAssetFolderPath(const Project::ProjectContext& projectContext, const std::filesystem::path& absoluteFolderPath)
    {
        const std::filesystem::path relativePath = absoluteFolderPath.lexically_normal().lexically_relative(projectContext.GetRootPath());
        const std::string relativeString = NormalizeProjectRelativePathString(relativePath);
        const std::string assetsRoot = GetAssetsRootRelativePath(projectContext);

        if (!relativeString.empty() && IsAssetFolderInsideAssetsRoot(relativeString, assetsRoot))
            return relativeString;

        return assetsRoot;
    }

    std::string GetAssetBrowserFolderForRecord(const Project::ProjectContext& projectContext, const Project::AssetRecord& record)
    {
        const std::string assetsRoot = GetAssetsRootRelativePath(projectContext);

        // Keep built-in, pathless, and external records accessible from the
        // Assets root until dedicated source categories/diagnostics are added.
        if (record.BuiltIn || record.Path.empty()) return assetsRoot;

        const std::filesystem::path resolvedAssetPath = ResolveDisplayPath(projectContext, record.Path);
        if (resolvedAssetPath.empty()) return assetsRoot;

        const std::filesystem::path relativeParentPath =
            resolvedAssetPath
                .parent_path()
                .lexically_relative(projectContext.GetRootPath());

        const std::string relativeFolder = NormalizeProjectRelativePathString(relativeParentPath);
        if (!relativeFolder.empty() && IsAssetFolderInsideAssetsRoot(relativeFolder, assetsRoot))
            return relativeFolder;

        return assetsRoot;
    }

    void ValidateCurrentAssetFolder(const Project::ProjectContext& projectContext, std::string& currentAssetFolder)
    {
        const std::string assetsRoot = GetAssetsRootRelativePath(projectContext);
        if (currentAssetFolder.empty())
        {
            currentAssetFolder = assetsRoot;
            return;
        }

        currentAssetFolder = NormalizeProjectRelativePathString(currentAssetFolder);
        if (!IsAssetFolderInsideAssetsRoot(currentAssetFolder, assetsRoot))
        {
            currentAssetFolder = assetsRoot;
            return;
        }

        std::error_code ec;
        const std::filesystem::path selectedFolderPath = ResolveAssetFolderPath(projectContext, currentAssetFolder);
        if (!std::filesystem::exists(selectedFolderPath, ec) || !std::filesystem::is_directory(selectedFolderPath, ec))
            currentAssetFolder = assetsRoot;
    }

    std::string GetParentAssetFolder(const Project::ProjectContext& projectContext, const std::string& currentAssetFolder)
    {
        const std::string assetsRoot = GetAssetsRootRelativePath(projectContext);
        const std::string normalizedFolder = currentAssetFolder.empty()
            ? assetsRoot
            : NormalizeProjectRelativePathString(currentAssetFolder);

        if (!IsAssetFolderInsideAssetsRoot(normalizedFolder, assetsRoot) || normalizedFolder == assetsRoot)
            return assetsRoot;

        const std::string parentFolder = NormalizeProjectRelativePathString(std::filesystem::path(normalizedFolder).parent_path());
        if (!IsAssetFolderInsideAssetsRoot(parentFolder, assetsRoot))
            return assetsRoot;

        return parentFolder;
    }

    std::vector<std::string> BuildAssetFolderBreadcrumbs(const Project::ProjectContext& projectContext, const std::string& currentAssetFolder)
    {
        const std::string assetsRoot = GetAssetsRootRelativePath(projectContext);
        std::string normalizedFolder = currentAssetFolder.empty()
            ? assetsRoot
            : NormalizeProjectRelativePathString(currentAssetFolder);

        if (!IsAssetFolderInsideAssetsRoot(normalizedFolder, assetsRoot))
            normalizedFolder = assetsRoot;

        std::vector<std::string> breadcrumbs{ assetsRoot };
        if (normalizedFolder == assetsRoot) return breadcrumbs;

        const std::filesystem::path relativeFolderPath = std::filesystem::path(normalizedFolder).lexically_relative(std::filesystem::path(assetsRoot));
        std::filesystem::path cumulativePath = assetsRoot;

        for (const std::filesystem::path& segment : relativeFolderPath)
        {
            cumulativePath /= segment;
            const std::string breadcrumbPath = NormalizeProjectRelativePathString(cumulativePath);
            if (!breadcrumbPath.empty()) breadcrumbs.push_back(breadcrumbPath);
        }
        return breadcrumbs;
    }

    std::filesystem::path ResolveDisplayPath(const Project::ProjectContext& projectContext, const std::string& assetPath)
    {
        std::filesystem::path path = assetPath;
        if (path.empty()) return {};
        if (path.is_absolute()) return path.lexically_normal();
        return (projectContext.GetRootPath() / path).lexically_normal();
    }

    bool DoesAssetPathExist(const Project::ProjectContext& projectContext, const std::string& assetPath)
    {
        const std::filesystem::path resolvedPath = ResolveDisplayPath(projectContext, assetPath);
        return !resolvedPath.empty() && std::filesystem::exists(resolvedPath);
    }

    std::string MakeStatusLabel(const Project::AssetRecord& record, const Vulkan::MeshLibrary& meshLibrary, const Vulkan::TextureLibrary& textureLibrary)
    {
        if (record.Type == Project::AssetType::Mesh)
        {
            if (meshLibrary.TryGet(record.ID))  return "Loaded";
            if (record.BuiltIn)                 return "Missing Built-In";
            return "Registered";
        }

        if (record.Type == Project::AssetType::Texture)
        {
            const auto textureStats = textureLibrary.GetStats(record.ID);
            if (textureStats.has_value())               return textureStats->GpuReady ? "GPU Ready" : "Loaded Metadata";
            if (record.Path.empty() && !record.BuiltIn) return "Missing Path";
            return "Registered";
        }

        return "Unknown";
    }

    std::string MakeAssetDisplayName(const Project::AssetRecord& record)
    {
        if (!record.Path.empty())
        {
            const std::string filename = std::filesystem::path(record.Path).filename().string();
            if (!filename.empty()) return filename;
        }
        return record.ID.empty() ? "<unnamed>" : record.ID;
    }

    bool IsAssetReadyStatus(const std::string& status)
    {
        return status == "Loaded" ||
               status == "GPU Ready" ||
               status == "Loaded Metadata";
    }

    bool IsAssetMissingStatus(const std::string& status)
    {
        return status == "Missing Built-In" ||
               status == "Missing Path";
    }

    std::string MakeAssetBrowserStateLabel(const Project::AssetRecord& record, const std::string& status, const bool runtimeModeActive, const std::uint32_t referenceCount)
    {
        if (IsAssetMissingStatus(status)) return "Missing";
        const std::string readiness = IsAssetReadyStatus(status) ? "Ready" : status;

        if (record.BuiltIn)     return "Built-in · " + readiness;
        if (referenceCount > 0) return readiness + " · In Use";
        if (runtimeModeActive)  return readiness + " · Runtime";
        return readiness + " · Removable";
    }

    std::uint32_t CountMeshReferences(const Scene::Scene& scene, const std::string& meshID)
    {
        std::uint32_t count = 0;
        for (const auto& actor : scene.GetActors())
        {
            if (!actor.StaticMesh.has_value()) continue;
            if (actor.StaticMesh->MeshResourceID == meshID) ++count;
        }
        return count;
    }
}
#include "Renderer/UI/Assets/ImGuiAssetBrowserUtils.h"

#include <algorithm>
#include <cctype>

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

    std::vector<const Project::AssetRecord*> BuildVisibleAssetList(
        const Project::AssetRegistry& registry, const AssetBrowserTypeFilter typeFilter, const std::string& searchQuery)
    {
        std::vector<const Project::AssetRecord*> visibleAssets;
        visibleAssets.reserve(registry.Records.size());

        for (const Project::AssetRecord& record : registry.Records)
        {
            if (!record.IsValid()) continue;
            if (!MatchesTypeFilter(record, typeFilter)) continue;
            if (!MatchesAssetSearch(record, searchQuery)) continue;
            visibleAssets.push_back(&record);
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
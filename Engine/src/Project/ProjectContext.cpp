#include "Quiet/Project/ProjectContext.h"

#include <filesystem>
#include <utility>
#include <algorithm>
#include <string_view>
#include <cctype>

namespace
{
    std::string TrimCopy(std::string value)
    {
        const auto isSpace = [](unsigned char c) { return std::isspace(c) != 0; };
        while (!value.empty() && isSpace(static_cast<unsigned char>(value.front())))
        {
            value.erase(value.begin());
        }
        while (!value.empty() && isSpace(static_cast<unsigned char>(value.back())))
        {
            value.pop_back();
        }
        return value;
    }

    bool IsValidAssetID(const std::string_view id)
    {
        if (id.empty()) return false;
        if (id == "." || id == "..") return false;
        static constexpr std::string_view invalidChars = "\\/:*?\"<>|";
        return id.find_first_of(invalidChars) == std::string_view::npos;
    }
}

namespace Quiet::Project
{
    bool ProjectContext::LoadOrCreateAssetRegistry()
    {
        AssetRegistry registry{};

        const std::filesystem::path registryPath = GetAssetRegistryPath();
        if (std::filesystem::exists(registryPath))
        {
            if (!AssetRegistryIO::LoadFromFile(registryPath, registry))
                return false;
        }
        else
            registry = AssetRegistryIO::MakeDefault();

        AssetRegistryIO::EnsureBuiltInAssets(registry);
        m_AssetRegistry = std::move(registry);

        return AssetRegistryIO::SaveToFile(registryPath, m_AssetRegistry);
    }

    bool ProjectContext::RegisterMeshAsset(std::string assetID, std::filesystem::path assetPath, bool builtIn)
    {
        assetID = TrimCopy(std::move(assetID));
        if (!IsValidAssetID(assetID)) return false;

        assetPath = NormalizeAssetPath(std::move(assetPath));

        AssetRecord record{};
        record.ID = std::move(assetID);
        record.Type = AssetType::Mesh;
        record.Path = builtIn ? std::string{} : assetPath.generic_string();
        record.BuiltIn = builtIn;

        if (!AssetRegistryIO::AddOrUpdate(m_AssetRegistry, std::move(record)))
            return false;

        AssetRegistryIO::EnsureBuiltInAssets(m_AssetRegistry);
        return SaveAssetRegistry();
    }

    bool ProjectContext::RegisterTextureAsset(std::string assetID, std::filesystem::path assetPath, bool builtIn)
    {
        assetID = TrimCopy(std::move(assetID));
        if (!IsValidAssetID(assetID)) return false;

        assetPath = NormalizeAssetPath(std::move(assetPath));

        AssetRecord record{};
        record.ID = std::move(assetID);
        record.Type = AssetType::Texture;
        record.Path = builtIn ? std::string{} : assetPath.generic_string();
        record.BuiltIn = builtIn;

        if (!AssetRegistryIO::AddOrUpdate(m_AssetRegistry, std::move(record)))
            return false;

        AssetRegistryIO::EnsureBuiltInAssets(m_AssetRegistry);
        return SaveAssetRegistry();
    }

    bool ProjectContext::RemoveAssetByID(std::string_view assetID)
    {
        const AssetRecord* record = AssetRegistryIO::FindByID(m_AssetRegistry, assetID);
        if (!record) return false;
        if (record->BuiltIn) return false;
        if (!AssetRegistryIO::RemoveByID(m_AssetRegistry, assetID)) return false;

        AssetRegistryIO::EnsureBuiltInAssets(m_AssetRegistry);
        return SaveAssetRegistry();
    }

    std::filesystem::path ProjectContext::NormalizeAssetPath(std::filesystem::path assetPath) const
    {
        if (assetPath.empty()) return {};
        assetPath = assetPath.lexically_normal();

        if (assetPath.is_relative()) return assetPath.generic_string();
        const std::filesystem::path relativeToRoot = assetPath.lexically_relative(GetRootPath());

        if (!relativeToRoot.empty() && *relativeToRoot.begin() != "..")
            return relativeToRoot.lexically_normal().generic_string();

        return assetPath.lexically_normal().generic_string();
    }

    bool LoadOrCreateProject(ProjectContext& projectContext,
                             const std::filesystem::path& projectFilePath)
    {
        ProjectDescriptor descriptor{};

        if (std::filesystem::exists(projectFilePath))
        {
            if (!ProjectDescriptorIO::LoadFromFile(projectFilePath, descriptor))
                return false;
        }
        else
        {
            const std::string projectName = projectFilePath.stem().string();
            descriptor = ProjectDescriptorIO::MakeDefault(projectName);

            if (!ProjectDescriptorIO::SaveToFile(projectFilePath, descriptor))
                return false;
        }

        projectContext.Initialize(projectFilePath, std::move(descriptor));
        if (!projectContext.EnsureDirectoriesExist())
            return false;

        return projectContext.LoadOrCreateAssetRegistry();
    }
}
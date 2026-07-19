#pragma once

#include <algorithm>
#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include "AssetRegistry.h"
#include "ProjectDescriptor.h"

namespace Quiet::Project
{
    class ProjectContext
    {
    public:
        ProjectContext() = default;

        void Initialize(std::filesystem::path projectFilePath, ProjectDescriptor descriptor)
        {
            m_ProjectFilePath = std::move(projectFilePath);
            m_RootPath = m_ProjectFilePath.parent_path();
            m_ProjectDescriptor = std::move(descriptor);
            ResetActiveScenePath();
        }

        [[nodiscard]] const std::filesystem::path& GetRootPath() const { return m_RootPath; }
        [[nodiscard]] const std::filesystem::path& GetProjectFilePath() const { return m_ProjectFilePath; }
        [[nodiscard]] const ProjectDescriptor& GetProjectDescriptor() const { return m_ProjectDescriptor; }
        [[nodiscard]] const std::string& GetProjectName() const { return m_ProjectDescriptor.Name; }

        [[nodiscard]] std::filesystem::path GetScenesPath() const { return m_RootPath / m_ProjectDescriptor.ScenesDirectory; }
        [[nodiscard]] std::filesystem::path GetAssetsPath() const { return m_RootPath / m_ProjectDescriptor.AssetsDirectory; }
        [[nodiscard]] std::filesystem::path GetCachePath() const { return m_RootPath / m_ProjectDescriptor.CacheDirectory; }
        [[nodiscard]] std::filesystem::path GetStartupScenePath() const { return GetScenesPath() / m_ProjectDescriptor.StartupScene; }
        [[nodiscard]] const std::filesystem::path& GetActiveScenePath() const { return m_ActiveScenePath; }

        [[nodiscard]] std::filesystem::path GetAssetRegistryPath() const { return m_RootPath / "AssetRegistry.json"; }
        [[nodiscard]] const AssetRegistry& GetAssetRegistry() const { return m_AssetRegistry; }
        [[nodiscard]] AssetRegistry& GetAssetRegistry() { return m_AssetRegistry; }
        [[nodiscard]] std::vector<std::string> EnumerateMeshAssetIDs() const { return AssetRegistryIO::GetAssetIDsByType(m_AssetRegistry, AssetType::Mesh); }
        [[nodiscard]] std::vector<std::string> EnumerateTextureAssetIDs() const {return AssetRegistryIO::GetAssetIDsByType(m_AssetRegistry, AssetType::Texture); }

        void SetActiveScenePath(std::filesystem::path path)
        {
            if (path.empty())
            {
                ResetActiveScenePath();
                return;
            }
            if (path.is_relative()) m_ActiveScenePath = GetScenesPath() / path;
            else m_ActiveScenePath = std::move(path);
        }

        void ResetActiveScenePath()
        {
            m_ActiveScenePath = GetStartupScenePath();
        }

        void SetStartupScene(std::filesystem::path relativeScenePath)
        {
            if (relativeScenePath.empty()) return;
            relativeScenePath = relativeScenePath.lexically_normal();
            m_ProjectDescriptor.StartupScene = relativeScenePath.generic_string();
        }

        [[nodiscard]] std::filesystem::path MakeSceneRelativePath(const std::filesystem::path& scenePath) const
        {
            if (scenePath.empty()) return {};
            if (scenePath.is_relative()) return scenePath.lexically_normal();

            const std::filesystem::path relative = scenePath.lexically_relative(GetScenesPath());
            if (!relative.empty()) return relative.lexically_normal();

            return scenePath.filename();
        }

        [[nodiscard]] std::vector<std::filesystem::path> EnumerateSceneFiles() const
        {
            std::vector<std::filesystem::path> result;
            std::error_code ec;
            if (!std::filesystem::exists(GetScenesPath(), ec)) return result;

            for (std::filesystem::recursive_directory_iterator it(GetScenesPath(), ec), end; it != end; it.increment(ec))
            {
                if (ec) break;
                if (!it->is_regular_file(ec)) continue;

                const std::filesystem::path filePath = it->path();
                if (filePath.extension() == ".json")
                {
                    result.push_back(MakeSceneRelativePath(filePath));
                }
            }

            std::sort(result.begin(), result.end());
            return result;
        }

        [[nodiscard]] bool SaveDescriptor() const
        {
            return ProjectDescriptorIO::SaveToFile(m_ProjectFilePath, m_ProjectDescriptor);
        }

        [[nodiscard]] bool LoadOrCreateAssetRegistry();

        [[nodiscard]] bool SaveAssetRegistry() const
        {
            return AssetRegistryIO::SaveToFile(GetAssetRegistryPath(), m_AssetRegistry);
        }

        [[nodiscard]] bool RegisterMeshAsset(std::string assetID, std::filesystem::path assetPath, bool builtIn = false);
        [[nodiscard]] bool RegisterTextureAsset(std::string assetID, std::filesystem::path assetPath, bool builtIn = false);
        [[nodiscard]] bool RemoveAssetByID(std::string_view assetID);
        [[nodiscard]] bool RemoveAssetsByID(const std::vector<std::string>& assetIDs, std::string& outErrorMessage);
        [[nodiscard]] bool RenameAssetFile(std::string_view assetID, std::string newBaseName, std::filesystem::path& outRenamedAssetPath, std::string& outErrorMessage);

        // Folders
        [[nodiscard]] bool CreateAssetFolder(
            const std::filesystem::path& parentFolder,
            std::string folderName,
            std::filesystem::path& outCreatedFolder,
            std::string& outErrorMessage);

        [[nodiscard]] bool RenameAssetFolder(
            const std::filesystem::path& folder,
            std::string newName,
            std::filesystem::path& outRenamedFolder,
            std::string& outErrorMessage);

        [[nodiscard]] bool DeleteEmptyAssetFolder(
            const std::filesystem::path& folder,
            std::string& outErrorMessage);

        [[nodiscard]] bool MoveAssetToFolder(
            std::string_view assetID,
            const std::filesystem::path& destinationFolder,
            std::filesystem::path& outMovedAssetPath,
            std::string& outErrorMessage);

        [[nodiscard]] bool EnsureDirectoriesExist() const
        {
            std::error_code ec;
            std::filesystem::create_directories(GetScenesPath(), ec);
            if (ec) return false;

            std::filesystem::create_directories(GetAssetsPath(), ec);
            if (ec) return false;

            std::filesystem::create_directories(GetCachePath(), ec);
            return !ec;
        }

        [[nodiscard]] std::filesystem::path NormalizeAssetPath(std::filesystem::path assetPath) const;

    private:
        void NormalizeAssetRegistryPaths();

        std::filesystem::path m_RootPath;
        std::filesystem::path m_ProjectFilePath;
        std::filesystem::path m_ActiveScenePath;
        ProjectDescriptor m_ProjectDescriptor{};
        AssetRegistry m_AssetRegistry{};
    };

    bool LoadOrCreateProject(ProjectContext& projectContext, const std::filesystem::path& projectFilePath);
}
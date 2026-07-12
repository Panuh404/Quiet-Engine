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
        const auto isSpace = [](const unsigned char character){ return std::isspace(character) != 0; };
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
        static constexpr std::string_view invalidCharacters = "\\/:*?\"<>|";
        return id.find_first_of(invalidCharacters) == std::string_view::npos;
    }

    std::string ToUpperCopy(std::string value)
    {
        std::transform(
            value.begin(),
            value.end(),
            value.begin(),
            [](const unsigned char character)
            {
                return static_cast<char>(
                    std::toupper(character)
                );
            }
        );
        return value;
    }

    bool IsReservedWindowsFolderName(const std::string& folderName)
    {
        const std::string upperName = ToUpperCopy(folderName);
        const std::size_t extensionPosition = upperName.find('.');
        const std::string baseName = upperName.substr(0, extensionPosition);

        if (baseName == "CON" || baseName == "PRN" || baseName == "AUX" || baseName == "NUL") return true;

        if (baseName.size() == 4)
        {
            const bool numberedDevice =
                baseName.compare(0, 3, "COM") == 0 ||
                baseName.compare(0, 3, "LPT") == 0;
            const char suffix = baseName[3];
            if (numberedDevice && suffix >= '1' && suffix <= '9') return true;
        }
        return false;
    }

    bool IsValidAssetFolderName(const std::string& folderName)
    {
        if (folderName.empty() || folderName == "." || folderName == "..") return false;
        static constexpr std::string_view invalidCharacters = "\\/:*?\"<>|";

        if (folderName.find_first_of(invalidCharacters) != std::string::npos) return false;
        if (folderName.back() == '.') return false;

        return !IsReservedWindowsFolderName(folderName);
    }

    bool IsPathInsideOrEqual(const std::filesystem::path& candidatePath, const std::filesystem::path& rootPath)
    {
        const std::filesystem::path normalizedCandidate = candidatePath.lexically_normal();
        const std::filesystem::path normalizedRoot = rootPath.lexically_normal();

        if (normalizedCandidate == normalizedRoot) return true;

        const std::filesystem::path relativePath = normalizedCandidate.lexically_relative(normalizedRoot);

        if (relativePath.empty() || relativePath.is_absolute()) return false;

        const auto firstSegment = relativePath.begin();
        return firstSegment != relativePath.end() && *firstSegment != "..";
    }

    bool ResolveExistingAssetFolder(
        const Quiet::Project::ProjectContext& projectContext,
        const std::filesystem::path& requestedFolder,
        std::filesystem::path& outResolvedFolder,
        std::string& outErrorMessage)
    {
        outResolvedFolder.clear();
        outErrorMessage.clear();

        if (requestedFolder.empty())
        {
            outErrorMessage = "No asset folder was selected.";
            return false;
        }

        std::error_code ec;
        const std::filesystem::path assetsRoot = std::filesystem::weakly_canonical(projectContext.GetAssetsPath(), ec);
        if (ec)
        {
            outErrorMessage = "Failed to resolve the project Assets folder: " + ec.message();
            return false;
        }

        std::filesystem::path resolvedFolder = requestedFolder;
        if (resolvedFolder.is_relative())
        {
            resolvedFolder = projectContext.GetRootPath() / resolvedFolder;
        }

        resolvedFolder = std::filesystem::weakly_canonical(resolvedFolder, ec);
        if (ec)
        {
            outErrorMessage = "Failed to resolve the selected asset folder: " + ec.message();
            return false;
        }
        if (!std::filesystem::exists(resolvedFolder, ec) || ec)
        {
            outErrorMessage = "The selected asset folder does not exist.";
            return false;
        }
        if (!std::filesystem::is_directory(resolvedFolder, ec) || ec)
        {
            outErrorMessage = "The selected asset path is not a folder.";
            return false;
        }
        if (!IsPathInsideOrEqual(resolvedFolder, assetsRoot))
        {
            outErrorMessage = "The selected folder is outside the project Assets folder.";
            return false;
        }

        outResolvedFolder = resolvedFolder.lexically_normal();
        return true;
    }

    std::filesystem::path ResolveAssetRecordPath(const Quiet::Project::ProjectContext& projectContext, const std::string& recordPath)
    {
        std::filesystem::path resolvedPath = recordPath;
        if (resolvedPath.is_relative())
        {
            resolvedPath = projectContext.GetRootPath() / resolvedPath;
        }
        return resolvedPath.lexically_normal();
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

    bool ProjectContext::CreateAssetFolder(
        const std::filesystem::path &parentFolder,
        std::string folderName,
        std::filesystem::path &outCreatedFolder,
        std::string &outErrorMessage)
    {
        outCreatedFolder.clear();
        outErrorMessage.clear();

        folderName = TrimCopy(std::move(folderName));
        if (!IsValidAssetFolderName(folderName))
        {
            outErrorMessage =
                "Enter a valid folder name. Folder names cannot contain "
                "\\ / : * ? \" < > |, end with a period, or use a reserved "
                "system name.";
            return false;
        }

        std::filesystem::path resolvedParentFolder;
        if (!ResolveExistingAssetFolder(*this, parentFolder, resolvedParentFolder, outErrorMessage))
            return false;

        const std::filesystem::path destinationFolder = (resolvedParentFolder / folderName).lexically_normal();

        std::error_code ec;
        const bool destinationExists = std::filesystem::exists(destinationFolder, ec);
        if (ec)
        {
            outErrorMessage = "Failed to inspect the destination folder: " + ec.message();
            return false;
        }

        if (destinationExists)
        {
            outErrorMessage =  "A folder named '" + folderName + "' already exists here.";
            return false;
        }

        const bool created = std::filesystem::create_directory(destinationFolder, ec);

        if (!created || ec)
        {
            outErrorMessage = "Failed to create the folder";
            if (ec) outErrorMessage += ": " + ec.message();
            return false;
        }

        outCreatedFolder = NormalizeAssetPath(destinationFolder);
        return true;
    }

    bool ProjectContext::RenameAssetFolder(
        const std::filesystem::path &folder,
        std::string newName,
        std::filesystem::path &outRenamedFolder,
        std::string &outErrorMessage)
    {
        outRenamedFolder.clear();
        outErrorMessage.clear();

        newName = TrimCopy(std::move(newName));

        if (!IsValidAssetFolderName(newName))
        {
            outErrorMessage =
                "Enter a valid folder name. Folder names cannot contain "
                "\\ / : * ? \" < > |, end with a period, or use a reserved "
                "system name.";
            return false;
        }

        std::filesystem::path resolvedFolder;
        if (!ResolveExistingAssetFolder(*this, folder, resolvedFolder, outErrorMessage)) return false;

        std::error_code ec;
        const std::filesystem::path assetsRoot = std::filesystem::weakly_canonical(GetAssetsPath(), ec);
        if (ec)
        {
            outErrorMessage = "Failed to resolve the project Assets folder: " + ec.message();
            return false;
        }
        if (resolvedFolder == assetsRoot)
        {
            outErrorMessage = "The project Assets root cannot be renamed.";
            return false;
        }
        if (resolvedFolder.filename().string() == newName)
        {
            outErrorMessage = "The folder already uses this name.";
            return false;
        }

        const std::filesystem::path renamedFolder = (resolvedFolder.parent_path() / newName).lexically_normal();
        const bool destinationExists = std::filesystem::exists(renamedFolder, ec);
        if (ec)
        {
            outErrorMessage = "Failed to inspect the rename destination: " + ec.message();
            return false;
        }

        if (destinationExists)
        {
            outErrorMessage = "A folder named '" + newName + "' already exists here.";
            return false;
        }

        const AssetRegistry previousRegistry = m_AssetRegistry;
        AssetRegistry updatedRegistry = m_AssetRegistry;
        for (AssetRecord& record : updatedRegistry.Records)
        {
            if (record.BuiltIn || record.Path.empty()) continue;
            const std::filesystem::path currentAssetPath = ResolveAssetRecordPath(*this, record.Path);

            if (!IsPathInsideOrEqual(currentAssetPath, resolvedFolder)) continue;

            const std::filesystem::path relativeAssetPath = currentAssetPath.lexically_relative(resolvedFolder);
            const std::filesystem::path renamedAssetPath = (renamedFolder / relativeAssetPath).lexically_normal();

            record.Path = NormalizeAssetPath(renamedAssetPath).generic_string();
        }

        std::filesystem::rename(resolvedFolder, renamedFolder, ec );
        if (ec)
        {
            outErrorMessage = "Failed to rename the folder: " + ec.message();
            return false;
        }

        m_AssetRegistry = std::move(updatedRegistry);
        if (!SaveAssetRegistry())
        {
            m_AssetRegistry = previousRegistry;
            std::error_code rollbackError;
            std::filesystem::rename(renamedFolder, resolvedFolder, rollbackError);

            const bool registryRestored = SaveAssetRegistry();

            if (rollbackError)
            {
                outErrorMessage =
                    "The folder was renamed, but the asset registry could not "
                    "be saved and the filesystem rollback also failed: " +
                    rollbackError.message();
            }
            else if (!registryRestored)
            {
                outErrorMessage =
                    "The folder rename was rolled back, but the original asset "
                    "registry could not be restored on disk.";
            }
            else
            {
                outErrorMessage =
                    "The folder rename was rolled back because the asset "
                    "registry could not be saved.";
            }
            return false;
        }

        outRenamedFolder = NormalizeAssetPath(renamedFolder);
        return true;
    }

    bool ProjectContext::DeleteEmptyAssetFolder(const std::filesystem::path &folder, std::string &outErrorMessage)
    {
        outErrorMessage.clear();

        std::filesystem::path resolvedFolder;

        if (!ResolveExistingAssetFolder(*this, folder, resolvedFolder, outErrorMessage)) return false;

        std::error_code ec;
        const std::filesystem::path assetsRoot = std::filesystem::weakly_canonical(GetAssetsPath(), ec);
        if (ec)
        {
            outErrorMessage = "Failed to resolve the project Assets folder: " + ec.message();
            return false;
        }

        if (resolvedFolder == assetsRoot)
        {
            outErrorMessage = "The project Assets root cannot be deleted.";
            return false;
        }

        for (const AssetRecord& record : m_AssetRegistry.Records)
        {
            if (record.BuiltIn || record.Path.empty()) continue;
            const std::filesystem::path assetPath = ResolveAssetRecordPath(*this, record.Path);

            if (IsPathInsideOrEqual(assetPath, resolvedFolder))
            {
                outErrorMessage =
                    "The folder still contains the registered asset '" +
                    record.ID +
                    "'.";
                return false;
            }
        }

        const bool folderIsEmpty = std::filesystem::is_empty(resolvedFolder, ec);
        if (ec)
        {
            outErrorMessage = "Failed to inspect the folder contents: " + ec.message();
            return false;
        }
        if (!folderIsEmpty)
        {
            outErrorMessage =
                "Only empty folders can be deleted. Remove or move every "
                "file and subfolder first.";
            return false;
        }

        const bool removed = std::filesystem::remove(resolvedFolder, ec);
        if (!removed || ec)
        {
            outErrorMessage = "Failed to delete the folder";
            if (ec) outErrorMessage += ": " + ec.message();
            return false;
        }

        return true;
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

    bool LoadOrCreateProject(ProjectContext& projectContext, const std::filesystem::path& projectFilePath)
    {
        ProjectDescriptor descriptor{};

        if (std::filesystem::exists(projectFilePath))
        {
            if (!ProjectDescriptorIO::LoadFromFile(projectFilePath, descriptor)) return false;
        }
        else
        {
            const std::string projectName = projectFilePath.stem().string();
            descriptor = ProjectDescriptorIO::MakeDefault(projectName);
            if (!ProjectDescriptorIO::SaveToFile(projectFilePath, descriptor)) return false;
        }

        projectContext.Initialize(projectFilePath, std::move(descriptor));
        if (!projectContext.EnsureDirectoriesExist()) return false;
        return projectContext.LoadOrCreateAssetRegistry();
    }
}
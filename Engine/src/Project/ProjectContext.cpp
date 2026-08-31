#include "Quiet/Project/ProjectContext.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

#include "Quiet/Core/UUID.h"

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

    std::filesystem::path MakeAbsoluteNormalizedPath(std::filesystem::path path)
    {
        if (path.empty()) return {};
        std::error_code ec;
        if (path.is_relative())
        {
            const std::filesystem::path absolutePath = std::filesystem::absolute(path, ec);
            if (!ec) path = absolutePath;
            ec.clear();
        }

        const std::filesystem::path canonicalPath = std::filesystem::weakly_canonical(path, ec);
        if (!ec) return canonicalPath.lexically_normal();

        return path.lexically_normal();
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
            if (!AssetRegistryIO::LoadFromFile(registryPath, registry)) return false;
        }
        else
        {
            registry = AssetRegistryIO::MakeDefault();
        }
        AssetRegistryIO::EnsureBuiltInAssets(registry);
        m_AssetRegistry = std::move(registry);

        NormalizeAssetRegistryPaths();
        return SaveAssetRegistry();
    }

    bool ProjectContext::RegisterMeshAsset(std::string assetID, std::filesystem::path assetPath, bool builtIn)
    {
        assetID = TrimCopy(std::move(assetID));
        if (!IsValidAssetID(assetID)) return false;

        assetPath = NormalizeAssetPath(std::move(assetPath));
        if (!builtIn && assetPath.empty()) return false;

        AssetRecord record{};
        record.ID = std::move(assetID);
        record.Type = AssetType::Mesh;
        record.Path = builtIn ? std::string{} : assetPath.generic_string();
        record.BuiltIn = builtIn;

        AssetRegistry updatedRegistry = m_AssetRegistry;
        if (!AssetRegistryIO::AddOrUpdate(updatedRegistry, std::move(record))) return false;

        AssetRegistryIO::EnsureBuiltInAssets(updatedRegistry);
        const AssetRegistry previousRegistry = m_AssetRegistry;
        m_AssetRegistry = std::move(updatedRegistry);

        if (SaveAssetRegistry()) return true;
        m_AssetRegistry = previousRegistry;

        (void)SaveAssetRegistry();
        return false;
    }

    bool ProjectContext::RegisterTextureAsset(std::string assetID, std::filesystem::path assetPath, bool builtIn)
    {
        assetID = TrimCopy(std::move(assetID));
        if (!IsValidAssetID(assetID)) return false;

        assetPath = NormalizeAssetPath(std::move(assetPath));
        if (!builtIn && assetPath.empty()) return false;

        AssetRecord record{};
        record.ID = std::move(assetID);
        record.Type = AssetType::Texture;
        record.Path = builtIn ? std::string{} : assetPath.generic_string();
        record.BuiltIn = builtIn;

        AssetRegistry updatedRegistry = m_AssetRegistry;
        if (!AssetRegistryIO::AddOrUpdate(updatedRegistry, std::move(record))) return false;

        AssetRegistryIO::EnsureBuiltInAssets(updatedRegistry);
        const AssetRegistry previousRegistry = m_AssetRegistry;

        m_AssetRegistry = std::move(updatedRegistry);
        if (SaveAssetRegistry()) return true;

        m_AssetRegistry = previousRegistry;
        (void)SaveAssetRegistry();
        return false;
    }

    bool ProjectContext::RegisterAudioAsset(std::string assetID, std::filesystem::path assetPath, bool builtIn)
    {
        assetID = TrimCopy(std::move(assetID));
        if (!IsValidAssetID(assetID)) return false;

        assetPath = NormalizeAssetPath(std::move(assetPath));
        if (!builtIn && assetPath.empty()) return false;

        AssetRecord record{};
        record.ID = std::move(assetID);
        record.Type = AssetType::Audio;
        record.Path = builtIn ? std::string{} : assetPath.generic_string();
        record.BuiltIn = builtIn;

        AssetRegistry updatedRegistry = m_AssetRegistry;
        if (!AssetRegistryIO::AddOrUpdate(updatedRegistry, std::move(record))) return false;

        AssetRegistryIO::EnsureBuiltInAssets(updatedRegistry);

        const AssetRegistry previousRegistry = m_AssetRegistry;
        m_AssetRegistry = std::move(updatedRegistry);
        if (SaveAssetRegistry()) return true;
        m_AssetRegistry = previousRegistry;

        (void)SaveAssetRegistry();
        return false;
    }

    bool ProjectContext::RegisterScriptAsset(std::string assetID, std::filesystem::path assetPath, bool builtIn)
    {
        assetID = TrimCopy(std::move(assetID));
        if (!IsValidAssetID(assetID)) return false;

        assetPath = NormalizeAssetPath(std::move(assetPath));
        if (!builtIn && assetPath.empty()) return false;

        AssetRecord record{};
        record.ID = std::move(assetID);
        record.Type = AssetType::Script;
        record.Path = builtIn ? std::string{} : assetPath.generic_string();
        record.BuiltIn = builtIn;

        AssetRegistry updatedRegistry = m_AssetRegistry;
        if (!AssetRegistryIO::AddOrUpdate(updatedRegistry, std::move(record))) return false;

        AssetRegistryIO::EnsureBuiltInAssets(updatedRegistry);
        const AssetRegistry previousRegistry = m_AssetRegistry;
        m_AssetRegistry = std::move(updatedRegistry);

        if (SaveAssetRegistry()) return true;
        m_AssetRegistry = previousRegistry;
        (void)SaveAssetRegistry();

        return false;
    }

    bool ProjectContext::RemoveAssetByID(std::string_view assetID)
    {
        const AssetRecord* record = AssetRegistryIO::FindByID(m_AssetRegistry, assetID);
        if (!record || record->BuiltIn) return false;

        AssetRegistry updatedRegistry = m_AssetRegistry;
        if (!AssetRegistryIO::RemoveByID(updatedRegistry, assetID)) return false;
        AssetRegistryIO::EnsureBuiltInAssets(updatedRegistry);

        const AssetRegistry previousRegistry = m_AssetRegistry;
        m_AssetRegistry = std::move(updatedRegistry);

        if (SaveAssetRegistry()) return true;

        m_AssetRegistry = previousRegistry;
        (void)SaveAssetRegistry();
        return false;
    }

    bool ProjectContext::RemoveAssetsByID(const std::vector<std::string> &assetIDs, std::string &outErrorMessage)
    {
        outErrorMessage.clear();
        std::vector<std::string> normalizedAssetIDs;
        normalizedAssetIDs.reserve(assetIDs.size());

        for (const std::string& assetID : assetIDs)
        {
            if (assetID.empty()) continue;
            if (std::find(
                normalizedAssetIDs.begin(),
                normalizedAssetIDs.end(),
                assetID) == normalizedAssetIDs.end())
            {
                normalizedAssetIDs.push_back(assetID);
            }
        }

        if (normalizedAssetIDs.empty())
        {
            outErrorMessage = "No assets were selected for removal.";
            return false;
        }

        AssetRegistry updatedRegistry = m_AssetRegistry;
        for (const std::string& assetID : normalizedAssetIDs)
        {
            const AssetRecord* record = AssetRegistryIO::FindByID(updatedRegistry, assetID);
            if (!record)
            {
                outErrorMessage = "Asset '" + assetID + "' is no longer registered.";
                return false;
            }

            if (record->BuiltIn)
            {
                outErrorMessage = "Built-in asset '" + record->ID + "' cannot be removed.";
                return false;
            }

            if (!AssetRegistryIO::RemoveByID(updatedRegistry, assetID))
            {
                outErrorMessage = "Failed to remove asset '" + assetID + "' from the pending registry update.";
                return false;
            }
        }

        AssetRegistryIO::EnsureBuiltInAssets(updatedRegistry);
        const AssetRegistry previousRegistry = m_AssetRegistry;
        m_AssetRegistry = std::move(updatedRegistry);

        if (SaveAssetRegistry()) return true;
        m_AssetRegistry = previousRegistry;

        const bool registryRestored = SaveAssetRegistry();
        outErrorMessage = registryRestored
            ? "The registry update could not be saved. The previous registry was restored."
            : "The registry update could not be saved, and the previous registry could not be restored on disk.";

        return false;
    }

    bool ProjectContext::DeleteAssetsFromProject(const std::vector<std::string>& assetIDs, std::string& outErrorMessage, std::string& outWarningMessage)
    {
        outErrorMessage.clear();
        outWarningMessage.clear();

        std::vector<std::string> normalizedAssetIDs;
        normalizedAssetIDs.reserve(assetIDs.size());

        for (const std::string& assetID : assetIDs)
        {
            if (assetID.empty()) continue;
            if (std::find(normalizedAssetIDs.begin(), normalizedAssetIDs.end(), assetID) == normalizedAssetIDs.end())
            {
                normalizedAssetIDs.push_back(assetID);
            }
        }

        if (normalizedAssetIDs.empty())
        {
            outErrorMessage = "No assets were selected for deletion.";
            return false;
        }

        std::error_code ec;
        const std::filesystem::path assetsRoot = std::filesystem::weakly_canonical(GetAssetsPath(), ec);
        if (ec)
        {
            outErrorMessage = "Failed to resolve the project Assets folder: " + ec.message();
            return false;
        }

        struct PlannedAssetDelete
        {
            std::string AssetID{};
            std::filesystem::path SourcePath{};
            std::filesystem::path StagedPath{};
        };

        const std::unordered_set<std::string> selectedAssetIDs(normalizedAssetIDs.begin(), normalizedAssetIDs.end());
        const std::filesystem::path stagingParent = assetsRoot / ".quiet-delete-staging";
        const std::filesystem::path stagingRoot = stagingParent / Quiet::Core::UUIDToString(Core::UUIDGenerator::New());

        std::vector<PlannedAssetDelete> deletionPlan;
        deletionPlan.reserve(normalizedAssetIDs.size());

        std::unordered_set<std::string> plannedSourcePaths;
        AssetRegistry updatedRegistry = m_AssetRegistry;

        std::size_t stagingIndex = 0;

        for (const std::string& assetID : normalizedAssetIDs)
        {
            const AssetRecord* record = AssetRegistryIO::FindByID(m_AssetRegistry, assetID);
            if (!record)
            {
                outErrorMessage = "Asset '" + assetID + "' is no longer registered.";
                return false;
            }

            if (record->BuiltIn)
            {
                outErrorMessage = "Built-in asset '" + record->ID + "' cannot be deleted.";
                return false;
            }

            if (record->Path.empty())
            {
                outErrorMessage = "Asset '" + record->ID + "' has no physical source path. Use Remove from Registry instead.";
                return false;
            }

            const std::filesystem::path sourcePath = ResolveAssetRecordPath(*this, record->Path);
            ec.clear();
            if (!std::filesystem::exists(sourcePath, ec) || ec)
            {
                outErrorMessage = "The source file for asset '" + record->ID + "' does not exist. Use Remove from Registry instead.";
                return false;
            }

            ec.clear();
            if (!std::filesystem::is_regular_file(sourcePath, ec) || ec)
            {
                outErrorMessage = "The registered source for asset '" + record->ID + "' is not a regular file.";
                return false;
            }

            ec.clear();
            const std::filesystem::path canonicalSourcePath = std::filesystem::weakly_canonical(sourcePath, ec);
            if (ec)
            {
                outErrorMessage = "Failed to resolve the source file for asset '" + record->ID + "': " + ec.message();
                return false;
            }

            if (!IsPathInsideOrEqual(canonicalSourcePath, assetsRoot))
            {
                outErrorMessage = "External asset '" + record->ID + "' cannot be deleted through the project Assets browser. Use Remove from Registry instead.";
                return false;
            }

            const std::string sourcePathKey = canonicalSourcePath.generic_string();

            if (!plannedSourcePaths .insert(sourcePathKey).second)
            {
                outErrorMessage = "Multiple selected registry records reference the same physical source file: '" + canonicalSourcePath.generic_string() + "'.";
                return false;
            }

            for (const AssetRecord& otherRecord : m_AssetRegistry.Records)
            {
                if (otherRecord.ID == record->ID || selectedAssetIDs.contains(otherRecord.ID) || otherRecord.BuiltIn || otherRecord.Path.empty())
                    continue;

                const std::filesystem::path otherPath = MakeAbsoluteNormalizedPath(ResolveAssetRecordPath(*this, otherRecord.Path));
                if (otherPath == canonicalSourcePath)
                {
                    outErrorMessage = "The source file for asset '" + record->ID + "' is also registered as asset '" + otherRecord.ID + "'. Remove the duplicate registry record first.";
                    return false;
                }
            }

            const std::string stagedFilename = std::to_string(stagingIndex++) + "_" + canonicalSourcePath.filename().string();

            deletionPlan.push_back(
                PlannedAssetDelete{
                    record->ID,
                    canonicalSourcePath,
                    stagingRoot / stagedFilename
                }
            );

            if (!AssetRegistryIO::RemoveByID(updatedRegistry, record->ID))
            {
                outErrorMessage = "Failed to remove asset '" + record->ID + "' from the pending registry update.";
                return false;
            }
        }
        AssetRegistryIO::EnsureBuiltInAssets(updatedRegistry);

        ec.clear();
        std::filesystem::create_directories(stagingRoot,ec);
        if (ec)
        {
            outErrorMessage = "Failed to create the temporary deletion staging folder: " + ec.message();
            return false;
        }

        std::vector<PlannedAssetDelete> stagedFiles;
        stagedFiles.reserve(deletionPlan.size());

        const auto restoreStagedFiles = [](const std::vector<PlannedAssetDelete>& files)
            {
                std::string failures;

                for (auto iterator = files.rbegin(); iterator != files.rend(); ++iterator)
                {
                    std::error_code rollbackError;
                    std::filesystem::rename(iterator->StagedPath, iterator->SourcePath, rollbackError);

                    if (!rollbackError) continue;
                    if (!failures.empty()) failures += "\n";

                    failures += "• Failed to restore '" + iterator->SourcePath.generic_string() + "': " + rollbackError.message();
                }

                return failures;
            };

        for (const PlannedAssetDelete& plannedDelete : deletionPlan)
        {
            ec.clear();
            std::filesystem::rename(plannedDelete.SourcePath, plannedDelete.StagedPath, ec);
            if (!ec)
            {
                stagedFiles.push_back(plannedDelete);
                continue;
            }

            const std::string rollbackFailures = restoreStagedFiles(stagedFiles);
            std::error_code cleanupError;
            if (rollbackFailures.empty())
            {
                std::filesystem::remove_all(stagingRoot, cleanupError);
            }

            outErrorMessage = "Failed to stage asset '" + plannedDelete.AssetID + "' for deletion: " + ec.message();
            if (!rollbackFailures.empty())
            {
                outErrorMessage += "\nThe rollback was incomplete:\n" + rollbackFailures + "\n\nRecovery files were preserved at:\n" + stagingRoot.generic_string();
            }
            else if (cleanupError)
            {
                outErrorMessage += "\n• Failed to clean the staging folder: " + cleanupError.message();
            }

            return false;
        }

        const AssetRegistry previousRegistry = m_AssetRegistry;
        m_AssetRegistry = std::move(updatedRegistry);

        if (!SaveAssetRegistry())
        {
            m_AssetRegistry = previousRegistry;

            const std::string rollbackFailures = restoreStagedFiles(stagedFiles);
            const bool registryRestored = SaveAssetRegistry();

            std::error_code cleanupError;

            if (rollbackFailures.empty())
            {
                std::filesystem::remove_all(stagingRoot, cleanupError);
            }
            outErrorMessage =
                registryRestored
                    ? "The deletion was rolled back because the asset registry could not be saved."
                    : "The deletion was rolled back, but the previous asset registry could not be restored on disk.";

            if (!rollbackFailures.empty())
            {
                outErrorMessage += "\nThe filesystem rollback was incomplete:\n" + rollbackFailures + "\n\nRecovery files were preserved at:\n" + stagingRoot.generic_string();
            }
            else if (cleanupError)
            {
                outErrorMessage += "\n• Failed to clean the staging folder: " + cleanupError.message();
            }

            return false;
        }

        ec.clear();
        std::filesystem::remove_all(stagingRoot, ec);
        if (ec)
        {
            outWarningMessage = "The assets were removed from the project and registry, but their staged files could not be permanently erased. "
                "Clean this folder manually:\n" + stagingRoot.generic_string() + "\n\n" + ec.message();
        }

        std::error_code parentCleanupError;
        std::filesystem::remove(stagingParent, parentCleanupError);
        return true;
    }

    bool ProjectContext::RenameAssetFile(std::string_view assetID, std::string newBaseName, std::filesystem::path &outRenamedAssetPath, std::string &outErrorMessage)
    {
        outRenamedAssetPath.clear();
        outErrorMessage.clear();

        newBaseName = TrimCopy(std::move(newBaseName));
        if (!IsValidAssetFolderName(newBaseName))
        {
            outErrorMessage = "Enter a valid asset name. Names cannot contain \\ / : * ? \" < > |, end with a period, or use a reserved system name.";
            return false;
        }

        const AssetRecord* record = AssetRegistryIO::FindByID(m_AssetRegistry, assetID);
        if (!record)
        {
            outErrorMessage = "The selected asset is no longer registered.";
            return false;
        }
        if (record->BuiltIn)
        {
            outErrorMessage = "Built-in assets cannot be renamed.";
            return false;
        }
        if (record->Path.empty())
        {
            outErrorMessage = "The selected asset has no physical source path.";
            return false;
        }

        const std::filesystem::path sourcePath = ResolveAssetRecordPath(*this, record->Path).lexically_normal();
        std::error_code ec;
        if (!std::filesystem::exists(sourcePath, ec) || ec)
        {
            outErrorMessage = "The asset source file does not exist.";
            return false;
        }

        ec.clear();
        if (!std::filesystem::is_regular_file(sourcePath, ec) || ec)
        {
            outErrorMessage = "The registered asset path is not a regular file.";
            return false;
        }

        ec.clear();
        const std::filesystem::path assetsRoot = std::filesystem::weakly_canonical(GetAssetsPath(), ec);
        if (ec)
        {
            outErrorMessage = "Failed to resolve the project Assets folder: " + ec.message();
            return false;
        }

        ec.clear();
        const std::filesystem::path canonicalSourcePath = std::filesystem::weakly_canonical(sourcePath, ec);
        if (ec)
        {
            outErrorMessage = "Failed to resolve the asset source path: " + ec.message();
            return false;
        }

        if (!IsPathInsideOrEqual(canonicalSourcePath, assetsRoot))
        {
            outErrorMessage = "External assets cannot be renamed through the project Assets browser.";
            return false;
        }

        const std::string currentBaseName = sourcePath.stem().string();
        if (currentBaseName == newBaseName)
        {
            outErrorMessage = "The asset already uses this name.";
            return false;
        }

        const std::filesystem::path renamedFilename = newBaseName + sourcePath.extension().string();
        const std::filesystem::path renamedPath = (sourcePath.parent_path() / renamedFilename).lexically_normal();

        ec.clear();
        const bool destinationExists = std::filesystem::exists(renamedPath, ec);
        if (ec)
        {
            outErrorMessage = "Failed to inspect the rename destination: " + ec.message();
            return false;
        }

        if (destinationExists)
        {
            outErrorMessage = "A file named '" + renamedFilename.string() + "' already exists in this folder.";
            return false;
        }

        for (const AssetRecord& existingRecord : m_AssetRegistry.Records)
        {
            if (existingRecord.ID == assetID || existingRecord.BuiltIn || existingRecord.Path.empty()) continue;
            const std::filesystem::path existingPath = ResolveAssetRecordPath(*this, existingRecord.Path).lexically_normal();
            if (existingPath == renamedPath)
            {
                outErrorMessage = "The destination path is already registered as asset '" + existingRecord.ID + "'.";
                return false;
            }
        }

        const AssetRegistry previousRegistry = m_AssetRegistry;
        AssetRegistry updatedRegistry = m_AssetRegistry;

        const auto updatedRecord =
            std::find_if(
                updatedRegistry.Records.begin(),
                updatedRegistry.Records.end(),
                [&](const AssetRecord& candidate)
                {
                    return candidate.ID == assetID;
                }
            );

        if (updatedRecord == updatedRegistry.Records.end())
        {
            outErrorMessage = "The selected asset disappeared from the registry.";
            return false;
        }
        updatedRecord->Path = NormalizeAssetPath(renamedPath).generic_string();

        ec.clear();
        std::filesystem::rename(sourcePath, renamedPath, ec);
        if (ec)
        {
            outErrorMessage = "Failed to rename the asset file: " + ec.message();
            return false;
        }

        m_AssetRegistry = std::move(updatedRegistry);
        if (!SaveAssetRegistry())
        {
            m_AssetRegistry = previousRegistry;
            std::error_code rollbackError;
            std::filesystem::rename(renamedPath, sourcePath, rollbackError);

            const bool registryRestored = SaveAssetRegistry();
            if (rollbackError)
            {
                outErrorMessage = "The asset file was renamed, but the registry could not be saved and the filesystem rollback also failed: " + rollbackError.message();
            }
            else if (!registryRestored)
            {
                outErrorMessage = "The asset rename was rolled back, but the original registry could not be restored on disk.";
            }
            else
            {
                outErrorMessage = "The asset rename was rolled back because the registry could not be saved.";
            }
            return false;
        }

        outRenamedAssetPath = NormalizeAssetPath(renamedPath);
        return true;
    }

    bool ProjectContext::CreateAssetFolder(const std::filesystem::path &parentFolder, std::string folderName,
                                           std::filesystem::path &outCreatedFolder, std::string &outErrorMessage)
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

    bool ProjectContext::RenameAssetFolder(const std::filesystem::path &folder, std::string newName,
        std::filesystem::path &outRenamedFolder, std::string &outErrorMessage)
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

    bool ProjectContext::DeleteAssetFolderFromProject(
        const std::filesystem::path &folder,
        std::vector<std::string> &outRemovedAssetIDs,
        std::string &outErrorMessage,
        std::string &outWarningMessage)
    {
        outRemovedAssetIDs.clear();
        outErrorMessage.clear();
        outWarningMessage.clear();

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

        const std::filesystem::path stagingParent = assetsRoot / ".quiet-delete-staging";
        if (IsPathInsideOrEqual(resolvedFolder, stagingParent))
        {
            outErrorMessage = "Quiet's internal deletion staging folder cannot be deleted through the Asset Browser.";
            return false;
        }

        // Recursive folder deletion must never follow or remove
        // filesystem links that may reference data outside the project.
        for (std::filesystem::recursive_directory_iterator iterator(resolvedFolder, std::filesystem::directory_options::none, ec), end; iterator != end; iterator.increment(ec))
        {
            if (ec)
            {
                outErrorMessage = "Failed to inspect the folder contents: " + ec.message();
                return false;
            }

            const std::filesystem::file_status status = iterator->symlink_status(ec);
            if (ec)
            {
                outErrorMessage = "Failed to inspect '" + iterator->path().generic_string() + "': " + ec.message();
                return false;
            }

            if (std::filesystem::is_symlink(status))
            {
                outErrorMessage = "The folder contains a symbolic link and cannot be deleted safely:\n" + iterator->path().generic_string();
                return false;
            }
        }

        if (ec)
        {
            outErrorMessage = "Failed to inspect the folder contents: " + ec.message();
            return false;
        }

        AssetRegistry updatedRegistry = m_AssetRegistry;
        for (const AssetRecord& record : m_AssetRegistry.Records)
        {
            if (record.BuiltIn || record.Path.empty()) continue;
            const std::filesystem::path assetPath = MakeAbsoluteNormalizedPath(ResolveAssetRecordPath(*this, record.Path));
            if (!IsPathInsideOrEqual(assetPath, resolvedFolder)) continue;

            outRemovedAssetIDs.push_back(record.ID);

            if (!AssetRegistryIO::RemoveByID(updatedRegistry, record.ID))
            {
                outRemovedAssetIDs.clear();
                outErrorMessage = "Failed to remove asset '" + record.ID + "' from the pending registry update.";
                return false;
            }
        }

        AssetRegistryIO::EnsureBuiltInAssets(updatedRegistry);

        const std::filesystem::path stagingRoot = stagingParent / Quiet::Core::UUIDToString(Core::UUIDGenerator::New());
        const std::filesystem::path stagedFolder = stagingRoot / resolvedFolder.filename();

        ec.clear();
        std::filesystem::create_directories(stagingRoot, ec);
        if (ec)
        {
            outRemovedAssetIDs.clear();
            outErrorMessage = "Failed to create the temporary deletion staging folder: " + ec.message();
            return false;
        }

        ec.clear();
        std::filesystem::rename(resolvedFolder, stagedFolder, ec);
        if (ec)
        {
            outRemovedAssetIDs.clear();
            std::error_code cleanupError;
            std::filesystem::remove_all(stagingRoot, cleanupError);

            outErrorMessage = "Failed to stage the folder for deletion: " + ec.message();
            if (cleanupError)
            {
                outErrorMessage += "\n• Failed to clean the staging folder: " + cleanupError.message();
            }
            return false;
        }

        const AssetRegistry previousRegistry = m_AssetRegistry;
        if (!outRemovedAssetIDs.empty())
        {
            m_AssetRegistry = std::move(updatedRegistry);
            if (!SaveAssetRegistry())
            {
                m_AssetRegistry = previousRegistry;

                std::error_code rollbackError;
                std::filesystem::rename(stagedFolder, resolvedFolder, rollbackError);
                const bool registryRestored = SaveAssetRegistry();

                if (!rollbackError)
                {
                    std::error_code cleanupError;
                    std::filesystem::remove_all(stagingRoot, cleanupError);

                    outErrorMessage = registryRestored
                            ? "The folder deletion was rolled back because the asset registry could not be saved."
                            : "The folder deletion was rolled back, but the previous registry could not be restored on disk.";

                    if (cleanupError)
                    {
                        outErrorMessage += "\n• Failed to clean the staging folder: " + cleanupError.message();
                    }
                }
                else
                {
                    outErrorMessage = "The asset registry could not be saved, and the folder could not be restored:\n" +
                        rollbackError.message() + "\n\nRecovery data was preserved at:\n" + stagingRoot.generic_string();

                    if (!registryRestored)
                    {
                        outErrorMessage += "\n\nThe previous registry could not be restored on disk.";
                    }
                }

                outRemovedAssetIDs.clear();
                return false;
            }
        }

        ec.clear();
        std::filesystem::remove_all(stagingRoot, ec);
        if (ec)
        {
            outWarningMessage = "The folder and registry records were removed from the project, but the staged folder could not be "
                "permanently erased. Clean this folder manually:\n" + stagingRoot.generic_string() + "\n\n" + ec.message();
        }

        std::error_code parentCleanupError;
        std::filesystem::remove(stagingParent, parentCleanupError);

        return true;
    }

    bool ProjectContext::MoveAssetToFolder(
        std::string_view assetID,
        const std::filesystem::path &destinationFolder,
        std::filesystem::path &outMovedAssetPath,
        std::string &outErrorMessage)
    {
        outMovedAssetPath.clear();
        outErrorMessage.clear();

        const AssetRecord* record = AssetRegistryIO::FindByID(m_AssetRegistry, assetID);
        if (!record)
        {
            outErrorMessage = "The selected asset is no longer registered.";
            return false;
        }
        if (record->BuiltIn)
        {
            outErrorMessage = "Built-in assets cannot be moved.";
            return false;
        }
        if (record->Path.empty())
        {
            outErrorMessage = "The selected asset has no physical source path.";
            return false;
        }

        std::filesystem::path resolvedDestinationFolder;
        if (!ResolveExistingAssetFolder(*this, destinationFolder, resolvedDestinationFolder, outErrorMessage))
            return false;

        const std::filesystem::path sourcePath = ResolveAssetRecordPath(*this, record->Path);

        std::error_code ec;
        if (!std::filesystem::exists(sourcePath, ec) || ec)
        {
            outErrorMessage = "The asset source file does not exist.";
            return false;
        }

        ec.clear();
        if (!std::filesystem::is_regular_file(sourcePath, ec) || ec)
        {
            outErrorMessage = "The registered asset path is not a regular file.";
            return false;
        }

        ec.clear();
        const std::filesystem::path assetsRoot = std::filesystem::weakly_canonical(GetAssetsPath(), ec);
        if (ec)
        {
            outErrorMessage = "Failed to resolve the project Assets folder: " + ec.message();
            return false;
        }

        ec.clear();
        const std::filesystem::path canonicalSourcePath = std::filesystem::weakly_canonical(sourcePath, ec);
        if (ec)
        {
            outErrorMessage = "Failed to resolve the asset source path: " + ec.message();
            return false;
        }

        if (!IsPathInsideOrEqual(canonicalSourcePath, assetsRoot))
        {
            outErrorMessage = "External assets cannot be moved through the project Assets browser.";
            return false;
        }
        if (canonicalSourcePath.parent_path() == resolvedDestinationFolder)
        {
            outErrorMessage = "The asset is already stored in the selected folder.";
            return false;
        }

        const std::filesystem::path sourceFilename = sourcePath.filename();
        if (sourceFilename.empty())
        {
            outErrorMessage = "The asset source path has no valid filename.";
            return false;
        }

        const std::filesystem::path movedPath = (resolvedDestinationFolder / sourceFilename).lexically_normal();
        ec.clear();
        if (std::filesystem::exists(movedPath, ec))
        {
            if (ec)
            {
                outErrorMessage = "Failed to inspect the destination path: " + ec.message();
            }
            else
            {
                outErrorMessage = "A file named '" + sourceFilename.string() + "' already exists in the destination folder.";
            }
            return false;
        }

        if (ec)
        {
            outErrorMessage = "Failed to inspect the destination path: " + ec.message();
            return false;
        }

        for (const AssetRecord& existingRecord :
             m_AssetRegistry.Records)
        {
            if (existingRecord.ID == assetID ||
                existingRecord.BuiltIn ||
                existingRecord.Path.empty())
            {
                continue;
            }

            const std::filesystem::path existingPath = ResolveAssetRecordPath(*this, existingRecord.Path);
            if (existingPath.lexically_normal() == movedPath.lexically_normal())
            {
                outErrorMessage = "The destination path is already registered as asset '" + existingRecord.ID + "'.";
                return false;
            }
        }

        const AssetRegistry previousRegistry = m_AssetRegistry;
        AssetRegistry updatedRegistry = m_AssetRegistry;

        const auto updatedRecord =
            std::find_if(
                updatedRegistry.Records.begin(),
                updatedRegistry.Records.end(),
                [&](const AssetRecord& candidate)
                {
                    return candidate.ID == assetID;
                }
            );

        if (updatedRecord == updatedRegistry.Records.end())
        {
            outErrorMessage = "The selected asset disappeared from the registry.";
            return false;
        }

        updatedRecord->Path = NormalizeAssetPath(movedPath).generic_string();

        ec.clear();
        std::filesystem::rename(sourcePath, movedPath, ec);
        if (ec)
        {
            outErrorMessage = "Failed to move the asset file: " + ec.message();
            return false;
        }

        m_AssetRegistry = std::move(updatedRegistry);
        if (!SaveAssetRegistry())
        {
            m_AssetRegistry = previousRegistry;
            std::error_code rollbackError;
            std::filesystem::rename(movedPath, sourcePath, rollbackError);

            const bool registryRestored = SaveAssetRegistry();
            if (rollbackError)
            {
                outErrorMessage =
                    "The asset file was moved, but the registry could not "
                    "be saved and the filesystem rollback also failed: " +
                    rollbackError.message();
            }
            else if (!registryRestored)
            {
                outErrorMessage =
                    "The asset move was rolled back, but the original "
                    "registry could not be restored on disk.";
            }
            else
            {
                outErrorMessage =
                    "The asset move was rolled back because the registry "
                    "could not be saved.";
            }
            return false;
        }

        outMovedAssetPath = NormalizeAssetPath(movedPath);
        return true;
    }

    std::filesystem::path ProjectContext::NormalizeAssetPath(std::filesystem::path assetPath) const
    {
        if (assetPath.empty()) return {};

        const std::filesystem::path projectRoot = MakeAbsoluteNormalizedPath(GetRootPath());
        if (projectRoot.empty()) return assetPath.lexically_normal();
        if (assetPath.is_relative()) assetPath = projectRoot / assetPath;

        const std::filesystem::path resolvedAssetPath = MakeAbsoluteNormalizedPath(std::move(assetPath));
        if (resolvedAssetPath.empty()) return {};

        if (IsPathInsideOrEqual(resolvedAssetPath, projectRoot))
        {
            const std::filesystem::path relativePath = resolvedAssetPath
                    .lexically_relative(projectRoot)
                    .lexically_normal();

            if (relativePath == ".") return {};
            return relativePath;
        }
        return resolvedAssetPath;
    }

    void ProjectContext::NormalizeAssetRegistryPaths()
    {
        for (AssetRecord& record : m_AssetRegistry.Records)
        {
            if (record.BuiltIn)
            {
                record.Path.clear();
                continue;
            }
            if (record.Path.empty()) continue;

            const std::filesystem::path normalizedPath = NormalizeAssetPath(record.Path);
            if (!normalizedPath.empty())
            {
                record.Path = normalizedPath.generic_string();
            }
        }
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
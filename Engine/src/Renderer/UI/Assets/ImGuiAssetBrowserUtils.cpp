#include "Renderer/UI/Assets/ImGuiAssetBrowserUtils.h"

#include "Audio/AudioLibrary.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <filesystem>
#include <system_error>

namespace
{
    namespace Assets = Quiet::Renderer::UI::Assets;

    bool ContainsAssetID(const std::vector<std::string>& assetIDs, const std::string& assetID)
    {
        return std::find(
            assetIDs.begin(),
            assetIDs.end(),
            assetID) != assetIDs.end();
    }

    bool ContainsAssetRecordID(const std::vector<const Quiet::Project::AssetRecord*>& records, const std::string& assetID)
    {
        return std::any_of(
            records.begin(),
            records.end(),
            [&](const Quiet::Project::AssetRecord* record)
            {
                return record && record->ID == assetID;
            }
        );
    }

    void AddAssetIDIfMissing(std::vector<std::string>& assetIDs, const std::string& assetID)
    {
        if (!ContainsAssetID(assetIDs, assetID)) assetIDs.push_back(assetID);
    }

    bool IsSameOrDescendantFolder(const std::string& candidateFolder, const std::string& parentFolder)
    {
        if (candidateFolder == parentFolder) return true;
        return candidateFolder.rfind(parentFolder + "/", 0) == 0;
    }

    std::string RemapFolderPath(const std::string& folder, const std::string& previousFolder, const std::string& renamedFolder)
    {
        if (folder.empty()) return {};
        if (folder == previousFolder) return renamedFolder;

        const std::string previousPrefix = previousFolder + "/";

        if (folder.rfind(previousPrefix, 0) == 0)
        {
            return renamedFolder + folder.substr(previousFolder.size());
        }
        return folder;
    }

    void ClearContentSelection(Assets::AssetBrowserState& state)
    {
        state.SelectedContentFolder.clear();
        Assets::ClearAssetSelection(state);
    }

    void CollapseAdjacentHistoryEntries(Assets::AssetBrowserState& state)
    {
        if (state.FolderHistory.empty())
        {
            state.FolderHistoryIndex = 0;
            return;
        }

        const std::size_t previousIndex = std::min(state.FolderHistoryIndex,  state.FolderHistory.size() - 1);
        std::vector<std::string> collapsedHistory;
        collapsedHistory.reserve(state.FolderHistory.size());
        std::size_t collapsedIndex = 0;

        for (std::size_t index = 0; index < state.FolderHistory.size(); ++index)
        {
            const std::string& entry = state.FolderHistory[index];

            if (collapsedHistory.empty() || collapsedHistory.back() != entry)
                collapsedHistory.push_back(entry);

            if (index == previousIndex)
                collapsedIndex = collapsedHistory.size() - 1;
        }

        state.FolderHistory.swap(collapsedHistory);
        state.FolderHistoryIndex = std::min(collapsedIndex, state.FolderHistory.size() - 1);
    }
}

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
            case AssetBrowserTypeFilter::Audio:     return record.Type == Project::AssetType::Audio;
            case AssetBrowserTypeFilter::Scripts:   return record.Type == Project::AssetType::Script;
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

    bool MatchesAssetSource(const Project::AssetRecord& record, const AssetBrowserSource source)
    {
        switch (source)
        {
            case AssetBrowserSource::EngineBuiltIn:
                return record.BuiltIn;
            case AssetBrowserSource::Project:
            default:
                return !record.BuiltIn;
        }
    }

    bool MatchesAssetFolder(const Project::ProjectContext& projectContext, const Project::AssetRecord& record, const std::string& currentAssetFolder)
    {
        if (record.BuiltIn) return false;

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
            if (!MatchesAssetSource(record, AssetBrowserSource::Project)) continue;
            if (!MatchesAssetFolder(projectContext, record, currentAssetFolder)) continue;
            folderAssets.push_back(&record);
        }
        return folderAssets;
    }

    std::vector<const Project::AssetRecord*> BuildBuiltInAssetList(const Project::AssetRegistry& registry)
    {
        std::vector<const Project::AssetRecord*> builtInAssets;
        builtInAssets.reserve(registry.Records.size());

        for (const Project::AssetRecord& record : registry.Records)
        {
            if (!record.IsValid()) continue;
            if (!MatchesAssetSource(record, AssetBrowserSource::EngineBuiltIn)) continue;
            builtInAssets.push_back(&record);
        }
        return builtInAssets;
    }

    std::vector<const Project::AssetRecord*> BuildVisibleAssetList(
        const Project::ProjectContext& projectContext,
        const std::vector<const Project::AssetRecord*>& folderAssets,
        const AssetBrowserTypeFilter typeFilter,
        const AssetBrowserDiagnosticsFilter diagnosticsFilter,
        const std::string& searchQuery)
    {
        std::vector<const Project::AssetRecord*> visibleAssets;
        visibleAssets.reserve(folderAssets.size());

        for (const Project::AssetRecord* record : folderAssets)
        {
            if (!record || !record->IsValid()) continue;
            if (!MatchesTypeFilter(*record, typeFilter)) continue;
            if (!MatchesDiagnosticsFilter(projectContext, *record, diagnosticsFilter)) continue;
            if (!MatchesAssetSearch(*record, searchQuery)) continue;
            visibleAssets.push_back(record);
        }

        return visibleAssets;
    }

    std::size_t CountAssetByType(const Project::AssetRegistry& registry, const Project::AssetType type, const AssetBrowserSource source)
    {
        std::size_t count = 0;

        for (const Project::AssetRecord& record : registry.Records)
        {
            if (!record.IsValid()) continue;
            if (!MatchesAssetSource(record, source)) continue;
            if (record.Type == type) ++count;
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

    std::vector<AssetBrowserFolderEntry> BuildAssetChildFolderList(const Project::ProjectContext &projectContext, const std::string &parentFolder)
    {
        std::vector<AssetBrowserFolderEntry> childFolders;
        const std::filesystem::path resolvedParentFolder = ResolveAssetFolderPath(projectContext, parentFolder);

        std::error_code ec;
        if (!std::filesystem::exists(resolvedParentFolder, ec) || ec)
            return childFolders;

        ec.clear();
        if (!std::filesystem::is_directory(resolvedParentFolder, ec) || ec)
            return childFolders;

        for (std::filesystem::directory_iterator iterator(resolvedParentFolder, ec), end; iterator != end; iterator.increment(ec))
        {
            if (ec)
            {
                ec.clear();
                continue;
            }

            std::error_code entryError;
            if (!iterator->is_directory(entryError) || entryError)
                continue;

            const std::string folderName = iterator->path().filename().string();
            if (folderName.empty() || folderName.front() == '.')
                continue;

            childFolders.push_back(AssetBrowserFolderEntry{
                folderName,
                MakeProjectRelativeAssetFolderPath(projectContext, iterator->path())
            });
        }

        std::sort(
            childFolders.begin(),
            childFolders.end(),
            [](const AssetBrowserFolderEntry& left,
               const AssetBrowserFolderEntry& right)
            {
                return ToLowerCopy(left.Name) <
                       ToLowerCopy(right.Name);
            }
        );

        return childFolders;
    }

    std::vector<AssetBrowserFolderEntry> BuildVisibleAssetFolderList(const std::vector<AssetBrowserFolderEntry> &childFolders, const std::string &searchQuery)
    {
        if (searchQuery.empty()) return childFolders;

        std::vector<AssetBrowserFolderEntry> visibleFolders;
        visibleFolders.reserve(childFolders.size());

        for (const AssetBrowserFolderEntry& folder : childFolders)
        {
            if (ContainsCaseInsensitive(folder.Name, searchQuery) || ContainsCaseInsensitive(folder.RelativePath, searchQuery))
                visibleFolders.push_back(folder);
        }

        return visibleFolders;
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

        // Pathless and external project records remain accessible from the physical Assets root.
        // Built-ins are displayed through the virtual Engine source and only reach this physical-folder fallback during validation.
        if (record.BuiltIn || record.Path.empty()) return assetsRoot;

        const std::filesystem::path resolvedAssetPath = ResolveDisplayPath(projectContext, record.Path);
        if (resolvedAssetPath.empty()) return assetsRoot;

        const std::filesystem::path relativeParentPath = resolvedAssetPath.parent_path() .lexically_relative(projectContext.GetRootPath());

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

    bool DoesAssetFolderExist(const Project::ProjectContext& projectContext, const std::string& assetFolder)
    {
        const std::string assetsRoot = GetAssetsRootRelativePath(projectContext);
        const std::string normalizedFolder = NormalizeProjectRelativePathString(assetFolder);

        if (!IsAssetFolderInsideAssetsRoot(normalizedFolder, assetsRoot)) return false;
        const std::filesystem::path resolvedFolder = ResolveAssetFolderPath(projectContext, normalizedFolder);

        std::error_code ec;
        if (!std::filesystem::exists(resolvedFolder, ec) || ec) return false;
        ec.clear();

        return std::filesystem::is_directory(resolvedFolder, ec) && !ec;
    }

    void SynchronizeAssetFolderHistory(const Project::ProjectContext& projectContext, AssetBrowserState& state)
    {
        ValidateCurrentAssetFolder(projectContext, state.CurrentAssetFolder);
        auto& history = state.FolderHistory;

        if (history.empty())
        {
            history.push_back(state.CurrentAssetFolder);
            state.FolderHistoryIndex = 0;
            return;
        }

        state.FolderHistoryIndex = std::min(state.FolderHistoryIndex, history.size() - 1);
        const std::size_t previousIndex = state.FolderHistoryIndex;

        std::vector<std::string> validHistory;
        validHistory.reserve(history.size());

        bool previousEntrySurvived = false;
        std::size_t remappedIndex = 0;

        for (std::size_t index = 0; index < history.size(); ++index)
        {
            if (!DoesAssetFolderExist(projectContext, history[index])) continue;
            const std::size_t destinationIndex = validHistory.size();
            validHistory.push_back(history[index]);

            if (index == previousIndex)
            {
                previousEntrySurvived = true;
                remappedIndex = destinationIndex;
            }
        }

        history.swap(validHistory);
        if (history.empty())
        {
            history.push_back(state.CurrentAssetFolder);
            state.FolderHistoryIndex = 0;
            return;
        }

        if (previousEntrySurvived)
        {
            state.FolderHistoryIndex = remappedIndex;
        }
        else
        {
            const auto currentEntry =
                std::find(
                    history.begin(),
                    history.end(),
                    state.CurrentAssetFolder
                );

            if (currentEntry != history.end())
            {
                state.FolderHistoryIndex = static_cast<std::size_t>(std::distance(history.begin(), currentEntry));
            }
            else
            {
                history.push_back(state.CurrentAssetFolder);
                state.FolderHistoryIndex = history.size() - 1;
            }
        }

        if (history[state.FolderHistoryIndex] == state.CurrentAssetFolder) return;
        history.erase(
            history.begin() + static_cast<std::ptrdiff_t>(state.FolderHistoryIndex + 1),
            history.end()
        );

        if (history.empty() || history.back() != state.CurrentAssetFolder)
        {
            history.push_back(state.CurrentAssetFolder);
        }

        state.FolderHistoryIndex = history.size() - 1;
        ClearContentSelection(state);
    }

    bool CanNavigateAssetFolderBack(const AssetBrowserState& state)
    {
        return !state.FolderHistory.empty() && state.FolderHistoryIndex > 0;
    }

    bool CanNavigateAssetFolderForward(const AssetBrowserState& state)
    {
        return !state.FolderHistory.empty() && state.FolderHistoryIndex + 1 < state.FolderHistory.size();
    }

    void NavigateToAssetFolder(const Project::ProjectContext& projectContext, AssetBrowserState& state, const std::string& targetFolder)
    {
        SynchronizeAssetFolderHistory(projectContext, state);
        std::string validatedFolder = targetFolder;

        ValidateCurrentAssetFolder(projectContext, validatedFolder);

        if (validatedFolder == state.CurrentAssetFolder) return;
        state.CurrentAssetFolder = validatedFolder;

        ClearContentSelection(state);
        SynchronizeAssetFolderHistory(projectContext, state);
    }

    void NavigateAssetFolderBack(const Project::ProjectContext& projectContext, AssetBrowserState& state)
    {
        SynchronizeAssetFolderHistory(projectContext, state);
        if (!CanNavigateAssetFolderBack(state)) return;

        --state.FolderHistoryIndex;
        state.CurrentAssetFolder = state.FolderHistory[state.FolderHistoryIndex];
        ClearContentSelection(state);
    }

    void NavigateAssetFolderForward(const Project::ProjectContext& projectContext, AssetBrowserState& state)
    {
        SynchronizeAssetFolderHistory(projectContext, state);
        if (!CanNavigateAssetFolderForward(state)) return;

        ++state.FolderHistoryIndex;
        state.CurrentAssetFolder = state.FolderHistory[state.FolderHistoryIndex];
        ClearContentSelection(state);
    }

    void RemapAssetFolderHistoryAfterRename(AssetBrowserState& state, const std::string& previousFolder, const std::string& renamedFolder)
    {
        for (std::string& entry : state.FolderHistory)
        {
            entry = RemapFolderPath(entry, previousFolder, renamedFolder);
        }

        state.CurrentAssetFolder = RemapFolderPath(state.CurrentAssetFolder, previousFolder, renamedFolder);
        state.SelectedContentFolder = RemapFolderPath(state.SelectedContentFolder, previousFolder, renamedFolder);
        CollapseAdjacentHistoryEntries(state);
    }

    void RepairAssetFolderHistoryAfterDelete(AssetBrowserState& state, const std::string& deletedFolder, const std::string& fallbackFolder)
    {
        for (std::string& entry : state.FolderHistory)
        {
            if (IsSameOrDescendantFolder(entry, deletedFolder))
                entry = fallbackFolder;
        }
        if (IsSameOrDescendantFolder(state.CurrentAssetFolder, deletedFolder))
        {
            state.CurrentAssetFolder = fallbackFolder;
        }
        if (IsSameOrDescendantFolder(state.SelectedContentFolder, deletedFolder))
        {
            state.SelectedContentFolder.clear();
        }

        CollapseAdjacentHistoryEntries(state);
    }

    bool IsAssetSelected(const AssetBrowserState &state, const std::string &assetID)
    {
        return ContainsAssetID(state.SelectedAssetIDs, assetID);
    }

    void ClearAssetSelection(AssetBrowserState &state)
    {
        state.SelectedAssetIDs.clear();
        state.SelectionAnchorAssetID.clear();
        state.SelectedDetailsAssetID.clear();
    }

    void SelectSingleAsset(AssetBrowserState &state, const std::string &assetID)
    {
        state.SelectedAssetIDs.clear();

        if (assetID.empty())
        {
            state.SelectionAnchorAssetID.clear();
            state.SelectedDetailsAssetID.clear();
            return;
        }

        state.SelectedAssetIDs.push_back(assetID);
        state.SelectionAnchorAssetID = assetID;
        state.SelectedDetailsAssetID = assetID;
        state.SelectedContentFolder.clear();
    }

    void HandleAssetSelectionClick(AssetBrowserState &state, const std::vector<const Project::AssetRecord *> &orderedAssets,
        const std::string &assetID, bool toggleModifier, bool rangeModifier)
    {
        if (assetID.empty()) return;

        const auto clickedIterator = std::find_if(
            orderedAssets.begin(),
            orderedAssets.end(),
            [&](const Project::AssetRecord* record)
            {
                return record && record->ID == assetID;
            }
        );

        if (clickedIterator == orderedAssets.end()) return;
        if (rangeModifier && !state.SelectionAnchorAssetID.empty())
        {
            const auto anchorIterator = std::find_if(
                orderedAssets.begin(),
                orderedAssets.end(),
                [&](const Project::AssetRecord* record)
                {
                    return record && record->ID == state.SelectionAnchorAssetID;
                }
            );

            if (anchorIterator != orderedAssets.end())
            {
                const std::size_t clickedIndex = static_cast<std::size_t>(std::distance(orderedAssets.begin(), clickedIterator));
                const std::size_t anchorIndex = static_cast<std::size_t>(std::distance(orderedAssets.begin(), anchorIterator));

                const std::size_t rangeStart = std::min(clickedIndex, anchorIndex);
                const std::size_t rangeEnd = std::max(clickedIndex, anchorIndex);

                if (!toggleModifier) state.SelectedAssetIDs.clear();

                for (std::size_t index = rangeStart; index <= rangeEnd; ++index)
                {
                    const Project::AssetRecord* record = orderedAssets[index];
                    if (record)
                    {
                        AddAssetIDIfMissing(state.SelectedAssetIDs, record->ID);
                    }
                }

                state.SelectedDetailsAssetID = assetID;
                state.SelectedContentFolder.clear();
                return;
            }
        }

        if (!toggleModifier)
        {
            SelectSingleAsset(state, assetID);
            return;
        }

        const auto selectedIterator = std::find(
                state.SelectedAssetIDs.begin(),
                state.SelectedAssetIDs.end(),
                assetID
            );

        if (selectedIterator == state.SelectedAssetIDs.end())
        {
            state.SelectedAssetIDs.push_back(assetID);
            state.SelectedDetailsAssetID = assetID;
        }
        else
        {
            const bool removedPrimary = state.SelectedDetailsAssetID == assetID;
            state.SelectedAssetIDs.erase(selectedIterator);

            if (removedPrimary)
            {
                state.SelectedDetailsAssetID = state.SelectedAssetIDs.empty() ? std::string{} : state.SelectedAssetIDs.back();
            }
        }

        state.SelectionAnchorAssetID = state.SelectedDetailsAssetID;
        state.SelectedContentFolder.clear();
    }

    void RemoveAssetFromSelection(AssetBrowserState &state, const std::string &assetID)
    {
        state.SelectedAssetIDs.erase(std::remove(state.SelectedAssetIDs.begin(), state.SelectedAssetIDs.end(), assetID),state.SelectedAssetIDs.end());
        if (state.SelectionAnchorAssetID == assetID)
        {
            state.SelectionAnchorAssetID = state.SelectedAssetIDs.empty() ? std::string{} : state.SelectedAssetIDs.back();
        }

        if (state.SelectedDetailsAssetID == assetID)
        {
            state.SelectedDetailsAssetID = state.SelectedAssetIDs.empty() ? std::string{} : state.SelectedAssetIDs.back();
        }
    }

    void RepairAssetSelection(AssetBrowserState &state, const std::vector<const Project::AssetRecord *> &folderAssets)
    {
        state.SelectedAssetIDs.erase(std::remove_if(
            state.SelectedAssetIDs.begin(),
            state.SelectedAssetIDs.end(),
            [&](const std::string& assetID)
            {
                return !ContainsAssetRecordID(folderAssets, assetID);
            }),
           state.SelectedAssetIDs.end()
        );

        if (state.SelectedAssetIDs.empty())
        {
            ClearAssetSelection(state);
            return;
        }

        if (!IsAssetSelected(state, state.SelectedDetailsAssetID))
        {
            state.SelectedDetailsAssetID = state.SelectedAssetIDs.back();
        }

        if (!ContainsAssetRecordID(folderAssets, state.SelectionAnchorAssetID))
        {
            state.SelectionAnchorAssetID = state.SelectedDetailsAssetID;
        }
    }

    void SetAssetBrowserSource(AssetBrowserState& state, const AssetBrowserSource source)
    {
        if (state.CurrentSource == source) return;

        state.CurrentSource = source;
        state.SelectedContentFolder.clear();

        ClearAssetSelection(state);

        if (source == AssetBrowserSource::EngineBuiltIn)
        {
            // Built-in assets cannot have missing, external, or
            // pathless project-source diagnostics.
            state.DiagnosticsFilter = AssetBrowserDiagnosticsFilter::All;
        }
    }

    std::filesystem::path ResolveDisplayPath(const Project::ProjectContext& projectContext, const std::string& assetPath)
    {
        std::filesystem::path path = assetPath;
        if (path.empty()) return {};
        if (path.is_absolute()) return path.lexically_normal();
        return (projectContext.GetRootPath() / path).lexically_normal();
    }

    AssetPathDiagnostics InspectAssetPath(const Project::ProjectContext& projectContext, const Project::AssetRecord& record)
    {
        AssetPathDiagnostics diagnostics{};
        diagnostics.BuiltIn = record.BuiltIn;

        if (record.BuiltIn) return diagnostics;
        diagnostics.HasPath = !record.Path.empty();
        if (!diagnostics.HasPath) return diagnostics;

        const std::filesystem::path registryPath = std::filesystem::path(record.Path).lexically_normal();

        // external assets are stored as absolute paths.
        diagnostics.External = registryPath.is_absolute();
        diagnostics.ResolvedPath = ResolveDisplayPath(projectContext, record.Path);
        if (diagnostics.ResolvedPath.empty()) return diagnostics;

        std::error_code ec;
        const std::filesystem::file_status sourceStatus = std::filesystem::status(diagnostics.ResolvedPath, ec);
        if (ec) return diagnostics;

        diagnostics.Exists = std::filesystem::exists(sourceStatus);
        diagnostics.RegularFile = diagnostics.Exists && std::filesystem::is_regular_file(sourceStatus);
        return diagnostics;
    }

    std::string MakeAssetPathDiagnosticLabel(const AssetPathDiagnostics& diagnostics)
    {
        if (diagnostics.BuiltIn)        return "Built-in";
        if (!diagnostics.HasPath)       return "Missing Path";
        if (!diagnostics.Exists)        return "Missing File";
        if (!diagnostics.RegularFile)   return "Invalid Source";
        if (diagnostics.External)       return "External";
        return "Project";
    }

    std::string MakeAssetPathDiagnosticDescription(const AssetPathDiagnostics& diagnostics)
    {
        if (diagnostics.BuiltIn)
            return "This is an engine-managed built-in asset and does not use a project source path.";

        if (!diagnostics.HasPath)
            return "This registry record has no source path. Reimport and physical folder operations are unavailable.";

        if (!diagnostics.Exists)
            return diagnostics.External
                ? "The external source file does not exist at its registered path."
                : "The registered project source file does not exist on disk.";

        if (!diagnostics.RegularFile)
            return "The registered source path exists, but it does not point to a regular file.";

        if (diagnostics.External)
            return
                "This asset points outside the current project. It can be "
                "inspected and reimported, but it cannot be moved through "
                "the physical Assets browser.";

        return "The source file is available inside the current project.";
    }

    bool MatchesDiagnosticsFilter(const Project::ProjectContext& projectContext, const Project::AssetRecord& record, const AssetBrowserDiagnosticsFilter filter)
    {
        if (filter == AssetBrowserDiagnosticsFilter::All) return true;
        const AssetPathDiagnostics diagnostics = InspectAssetPath(projectContext, record);
        switch (filter)
        {
            case AssetBrowserDiagnosticsFilter::Issues:     return diagnostics.HasIssue();
            case AssetBrowserDiagnosticsFilter::Missing:    return diagnostics.IsMissing();
            case AssetBrowserDiagnosticsFilter::External:   return diagnostics.External;
            case AssetBrowserDiagnosticsFilter::All:
            default:                                        return true;
        }
    }

    AssetDiagnosticsCounts CountAssetDiagnostics(const Project::ProjectContext& projectContext, const AssetBrowserSource source)
    {
        AssetDiagnosticsCounts counts{};

        for (const Project::AssetRecord& record : projectContext.GetAssetRegistry().Records)
        {
            if (!record.IsValid()) continue;
            if (!MatchesAssetSource(record, source)) continue;

            const AssetPathDiagnostics diagnostics = InspectAssetPath(projectContext, record);
            if (diagnostics.HasIssue()) ++counts.Issues;
            if (diagnostics.IsMissing()) ++counts.Missing;
            if (diagnostics.External) ++counts.External;
        }
        return counts;
    }

    bool DoesAssetPathExist(const Project::ProjectContext& projectContext, const std::string& assetPath)
    {
        const std::filesystem::path resolvedPath = ResolveDisplayPath(projectContext, assetPath);
        return !resolvedPath.empty() && std::filesystem::exists(resolvedPath);
    }

    std::string MakeStatusLabel(const Project::ProjectContext& projectContext,
                                const Project::AssetRecord& record,
                                const Vulkan::MeshLibrary& meshLibrary,
                                const Vulkan::TextureLibrary& textureLibrary,
                                const Audio::AudioLibrary& audioLibrary)
    {
        const AssetPathDiagnostics diagnostics = InspectAssetPath(projectContext, record);
        if (!record.BuiltIn)
        {
            if (!diagnostics.HasPath)       return "Missing Path";
            if (!diagnostics.Exists)        return "Missing File";
            if (!diagnostics.RegularFile)   return "Invalid Source";
        }

        if (record.Type == Project::AssetType::Mesh)
        {
            if (meshLibrary.TryGet(record.ID))  return "Loaded";
            if (record.BuiltIn)                 return "Missing Built-In";
            return "Registered";
        }

        if (record.Type == Project::AssetType::Texture)
        {
            const auto textureStats = textureLibrary.GetStats(record.ID);
            if (textureStats.has_value())
            {
                return textureStats->GpuReady ? "GPU Ready" : "Loaded Metadata";
            }
            return "Registered";
        }
        if (record.Type == Project::AssetType::Audio)
        {
            return audioLibrary.Has(record.ID) ? "Loaded Metadata" : "Registered";
        }
        if (record.Type == Project::AssetType::Script)
        {
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
               status == "Missing Path" ||
               status == "Missing File" ||
               status == "Invalid Source";
    }

    std::string MakeAssetBrowserStateLabel(
        const AssetPathDiagnostics& diagnostics,
        const Project::AssetRecord& record,
        const std::string& status,
        const bool runtimeModeActive,
        const std::uint32_t referenceCount)
    {
        if (diagnostics.IsMissing())
        {
            std::string label = MakeAssetPathDiagnosticLabel(diagnostics);
            if (diagnostics.External) label += " · External";
            return label;
        }

        if (IsAssetMissingStatus(status)) return "Missing";
        const std::string readiness = IsAssetReadyStatus(status) ? "Ready" : status;

        if (diagnostics.External)   return "External · " + readiness;
        if (record.BuiltIn)         return "Built-in · " + readiness;
        if (referenceCount > 0)     return readiness + " · In Use";
        if (runtimeModeActive)      return readiness + " · Runtime";
        return readiness + " · Removable";
    }

    std::uint32_t CountAssetReferences(const Scene::Scene& scene, const Project::AssetRecord& record)
    {
        std::uint32_t count = 0;

        for (const Scene::Actor& actor : scene.GetActors())
        {
            bool referenced = false;
            switch (record.Type)
            {
                case Project::AssetType::Mesh:
                {
                    const Scene::StaticMeshComponent* staticMesh = actor.TryGetComponent<Scene::StaticMeshComponent>();
                    referenced = staticMesh && staticMesh->MeshResourceID == record.ID;
                    break;
                }

                case Project::AssetType::Texture:
                {
                    const Scene::MaterialComponent* material = actor.TryGetComponent<Scene::MaterialComponent>();
                    referenced = material && material->BaseColorTextureAssetID == record.ID;
                    if (!referenced)
                    {
                        const Scene::StaticMeshComponent* staticMesh = actor.TryGetComponent<Scene::StaticMeshComponent>();
                        if (staticMesh)
                        {
                            for (const Scene::MaterialComponent& slot : staticMesh->MaterialSlots)
                            {
                                if (slot.BaseColorTextureAssetID == record.ID)
                                {
                                    referenced = true;
                                    break;
                                }
                            }
                        }
                    }
                    break;
                }

                case Project::AssetType::Audio:
                {
                    const Scene::AudioSourceComponent* audioSource = actor.TryGetComponent<Scene::AudioSourceComponent>();
                    referenced = audioSource && audioSource->AudioAssetID == record.ID;
                    break;
                }

                case Project::AssetType::Script:
                {
                    // TODO: ScriptComponent :D
                    break;
                }

                case Project::AssetType::Unknown:
                default:
                    break;
            }

            if (referenced) ++count;
        }

        return count;
    }
}
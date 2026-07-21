#include "Renderer/UI/Assets/ImGuiAssetBrowserOperations.h"

#include <algorithm>
#include <filesystem>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "Quiet/Project/AssetRegistry.h"
#include "Renderer/UI/Assets/ImGuiAssetBrowserUtils.h"

namespace
{
    namespace Assets = Quiet::Renderer::UI::Assets;

    struct PlannedAssetMove
    {
        std::string AssetID{};
        std::string SourceFolder{};
        std::filesystem::path DestinationPath{};
    };

    struct PlannedAssetDuplicate
    {
        std::string NewAssetID{};

        Quiet::Project::AssetType Type = Quiet::Project::AssetType::Unknown;
        std::filesystem::path SourcePath{};
        std::filesystem::path DestinationPath{};
    };

    struct CompletedAssetDuplicate
    {
        std::string AssetID{};
        std::filesystem::path Path{};
    };

    std::vector<std::string> NormalizeAssetIDs(const std::vector<std::string>& assetIDs)
    {
        std::vector<std::string> normalized;
        normalized.reserve(assetIDs.size());

        for (const std::string& assetID : assetIDs)
        {
            if (assetID.empty()) continue;
            if (std::find(
                normalized.begin(),
                normalized.end(),
                assetID) == normalized.end())
            {
                normalized.push_back(assetID);
            }
        }

        return normalized;
    }

    bool IsDestinationRegisteredByAnotherAsset(
        const Quiet::Project::ProjectContext& projectContext,
        const std::string& movingAssetID,
        const std::filesystem::path& destinationPath,
        std::string& outConflictingAssetID)
    {
        outConflictingAssetID.clear();

        const std::filesystem::path normalizedDestination = destinationPath.lexically_normal();
        for (const Quiet::Project::AssetRecord& record : projectContext.GetAssetRegistry().Records)
        {
            if (record.ID == movingAssetID || record.BuiltIn || record.Path.empty())
                continue;

            const std::filesystem::path registeredPath = Assets::ResolveDisplayPath(projectContext, record.Path).lexically_normal();
            if (registeredPath == normalizedDestination)
            {
                outConflictingAssetID = record.ID;
                return true;
            }
        }

        return false;
    }

    bool IsAssetPathRegistered(const Quiet::Project::ProjectContext& projectContext, const std::filesystem::path& candidatePath)
    {
        const std::filesystem::path normalizedCandidate = candidatePath.lexically_normal();

        for (const Quiet::Project::AssetRecord& record : projectContext.GetAssetRegistry().Records)
        {
            if (record.BuiltIn || record.Path.empty()) continue;

            const std::filesystem::path registeredPath = Assets::ResolveDisplayPath(projectContext, record.Path).lexically_normal();
            if (registeredPath == normalizedCandidate) return true;
        }
        return false;
    }

    std::string MakeUniqueDuplicateAssetID(
        const Quiet::Project::ProjectContext& projectContext,
        const std::string& sourceAssetID,
        const std::unordered_set<std::string>& plannedAssetIDs)
    {
        for (std::size_t index = 1; index < 100000; ++index)
        {
            const std::string suffix = index == 1 ? "_Copy" : "_Copy" + std::to_string(index);
            const std::string candidate = sourceAssetID + suffix;

            if (plannedAssetIDs.contains(candidate)) continue;
            if (!Quiet::Project::AssetRegistryIO::FindByID(projectContext.GetAssetRegistry(), candidate)) return candidate;
        }
        return {};
    }

    bool TryMakeUniqueDuplicatePath(
        const Quiet::Project::ProjectContext& projectContext,
        const std::filesystem::path& targetFolder,
        const std::filesystem::path& sourceFilename,
        const std::unordered_set<std::string>& plannedPaths,
        std::filesystem::path& outPath,
        std::string& outErrorMessage)
    {
        outPath.clear();
        outErrorMessage.clear();

        const std::string sourceStem = sourceFilename.stem().string();
        const std::string sourceExtension = sourceFilename.extension().string();

        if (sourceStem.empty())
        {
            outErrorMessage = "The source file has no valid filename.";
            return false;
        }

        for (std::size_t index = 1; index < 100000; ++index)
        {
            const std::string suffix = index == 1 ? "_Copy" : "_Copy" + std::to_string(index);
            const std::filesystem::path candidate = (targetFolder / (sourceStem + suffix + sourceExtension)).lexically_normal();
            const std::string candidateKey = candidate.generic_string();

            if (plannedPaths.contains(candidateKey)) continue;

            std::error_code ec;
            const bool pathExists = std::filesystem::exists(candidate, ec);
            if (ec)
            {
                outErrorMessage = "Failed to inspect duplicate destination '" + candidate.generic_string() + "': " + ec.message();
                return false;
            }

            if (pathExists || IsAssetPathRegistered(projectContext, candidate)) continue;
            outPath = candidate;
            return true;
        }

        outErrorMessage = "Quiet could not generate a unique duplicate filename.";
        return false;
    }

    std::string MakeDuplicateCountLabel(const std::size_t count)
    {
        return count == 1 ? "1 asset" : std::to_string(count) + " assets";
    }

    std::string MakeMoveCountLabel(const std::size_t count)
    {
        if (count == 1) return "1 asset";
        return std::to_string(count) + " assets";
    }

    std::string MakeAssetClipboardProjectKey(const Quiet::Project::ProjectContext& projectContext)
    {
        return projectContext.GetRootPath().lexically_normal().generic_string();
    }

    bool StoreAssetClipboard(
        const Quiet::Project::ProjectContext& projectContext,
        Quiet::Editor::EditorState& editorState,
        Assets::AssetBrowserState& state,
        const Assets::AssetBrowserClipboardMode mode,
        const std::vector<std::string>& requestedAssetIDs)
    {
        if (mode == Assets::AssetBrowserClipboardMode::None) return false;

        if (editorState.Scene.Mode != Quiet::Editor::SceneMode::Edit)
        {
            editorState.AssetOperationFeedback.Set(Quiet::Editor::EditorFeedbackSeverity::Warning,
                "Clipboard operation rejected",
                "Asset operations are unavailable while runtime mode is active.");
            return false;
        }

        const std::vector<std::string> assetIDs = NormalizeAssetIDs(requestedAssetIDs);
        if (assetIDs.empty()) return false;

        for (const std::string& assetID : assetIDs)
        {
            const Quiet::Project::AssetRecord* record = Quiet::Project::AssetRegistryIO::FindByID(projectContext.GetAssetRegistry(), assetID);
            if (!record)
            {
                editorState.AssetOperationFeedback.Set(Quiet::Editor::EditorFeedbackSeverity::Error,
                    "Clipboard operation failed",
                    "Asset '" + assetID + "' is no longer registered.");
                return false;
            }

            if (record->BuiltIn)
            {
                editorState.AssetOperationFeedback.Set(Quiet::Editor::EditorFeedbackSeverity::Error,
                    "Clipboard operation failed",
                    "Built-in asset '" + record->ID + "' has no physical source file to copy or move.");
                return false;
            }

            const Assets::AssetPathDiagnostics diagnostics = Assets::InspectAssetPath(projectContext, *record);
            if (diagnostics.IsMissing())
            {
                editorState.AssetOperationFeedback.Set(Quiet::Editor::EditorFeedbackSeverity::Error,
                    "Clipboard operation failed",
                    "Asset '" + record->ID + "' does not have a valid physical source file.");
                return false;
            }

            if (mode == Assets::AssetBrowserClipboardMode::Move && diagnostics.External)
            {
                editorState.AssetOperationFeedback.Set(Quiet::Editor::EditorFeedbackSeverity::Error,
                    "Move preparation failed",
                    "External asset '" + record->ID + "' cannot be moved through the project Assets browser.");
                return false;
            }
        }

        state.Clipboard.Mode = mode;
        state.Clipboard.AssetIDs = assetIDs;
        state.Clipboard.ProjectRoot = MakeAssetClipboardProjectKey(projectContext);

        const bool copyMode = mode == Assets::AssetBrowserClipboardMode::Copy;
        editorState.AssetOperationFeedback.Set(Quiet::Editor::EditorFeedbackSeverity::Success,
            copyMode
                ? (assetIDs.size() == 1 ? "Asset copied" : "Assets copied")
                : (assetIDs.size() == 1 ? "Asset ready to move" : "Assets ready to move"),
            copyMode
                ? ("Copied " + MakeMoveCountLabel(assetIDs.size()) + " to the Asset Browser clipboard.")
                : ("Marked " + MakeMoveCountLabel(assetIDs.size()) + " for moving. Navigate to a destination folder and use Paste."));
        return true;
    }

    std::filesystem::path NormalizeOperationPath(std::filesystem::path path)
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

    bool IsOperationPathInsideOrEqual(const std::filesystem::path& candidatePath, const std::filesystem::path& rootPath)
    {
        const std::filesystem::path candidate = candidatePath.lexically_normal();
        const std::filesystem::path root = rootPath.lexically_normal();
        if (candidate == root) return true;

        const std::filesystem::path relative = candidate.lexically_relative(root);
        if (relative.empty() || relative.is_absolute()) return false;

        const auto firstSegment = relative.begin();
        return firstSegment != relative.end() && *firstSegment != "..";
    }
}

namespace Quiet::Renderer::UI::Assets
{
    bool ExecuteAssetMoveOperation(
        Project::ProjectContext& projectContext,
        Editor::EditorState& editorState,
        AssetBrowserState& state,
        const AssetBrowserAssetMoveRequest& request)
    {
        if (!request.IsValid()) return false;

        if (editorState.Scene.Mode != Editor::SceneMode::Edit)
        {
            editorState.AssetOperationFeedback.Set(Editor::EditorFeedbackSeverity::Warning,
                "Move rejected",
                "Assets cannot be moved while runtime mode is active.");

            return false;
        }

        const std::vector<std::string> assetIDs = NormalizeAssetIDs(request.AssetIDs);
        if (assetIDs.empty()) return false;

        const std::string targetFolder = NormalizeProjectRelativePathString(request.TargetFolder);

        if (!DoesAssetFolderExist(projectContext, targetFolder))
        {
            editorState.AssetOperationFeedback.Set(Editor::EditorFeedbackSeverity::Error,
                "Move failed",
                "The destination folder no longer exists.");
            return false;
        }

        const std::filesystem::path resolvedTargetFolder = ResolveAssetFolderPath(projectContext, targetFolder).lexically_normal();
        std::vector<PlannedAssetMove> plan;
        plan.reserve(assetIDs.size());

        std::unordered_set<std::string> plannedDestinationPaths;

        for (const std::string& assetID : assetIDs)
        {
            const Project::AssetRecord* record = Project::AssetRegistryIO::FindByID(projectContext.GetAssetRegistry(), assetID);
            if (!record)
            {
                editorState.AssetOperationFeedback.Set(Editor::EditorFeedbackSeverity::Error,
                    "Move failed",
                    "Asset '" + assetID + "' is no longer registered.");
                return false;
            }

            if (record->BuiltIn)
            {
                editorState.AssetOperationFeedback.Set(Editor::EditorFeedbackSeverity::Error,
                    "Move failed",
                    "Built-in asset '" + record->ID + "' cannot be moved.");
                return false;
            }

            const AssetPathDiagnostics diagnostics = InspectAssetPath(projectContext, *record);
            if (diagnostics.IsMissing())
            {
                editorState.AssetOperationFeedback.Set(Editor::EditorFeedbackSeverity::Error,
                    "Move failed",
                    "Asset '" + record->ID + "' does not have a valid physical source file.");
                return false;
            }

            if (diagnostics.External)
            {
                editorState.AssetOperationFeedback.Set(Editor::EditorFeedbackSeverity::Error,
                    "Move failed",
                    "External asset '" + record->ID + "' cannot be moved through the project Assets browser.");
                return false;
            }

            const std::filesystem::path sourcePath = diagnostics.ResolvedPath.lexically_normal();
            if (sourcePath.parent_path() == resolvedTargetFolder)
            {
                editorState.AssetOperationFeedback.Set(Editor::EditorFeedbackSeverity::Warning,
                    "Move cancelled",
                    "Asset '" + record->ID + "' is already stored in the destination folder.");
                return false;
            }

            const std::filesystem::path filename = sourcePath.filename();
            if (filename.empty())
            {
                editorState.AssetOperationFeedback.Set(Editor::EditorFeedbackSeverity::Error,
                    "Move failed",
                    "Asset '" + record->ID + "' has no valid source filename.");
                return false;
            }

            const std::filesystem::path destinationPath = (resolvedTargetFolder / filename).lexically_normal();
            std::error_code ec;
            if (std::filesystem::exists(destinationPath, ec))
            {
                editorState.AssetOperationFeedback.Set(Editor::EditorFeedbackSeverity::Error,
                    "Move failed",
                    "A file named '" + filename.string() + "' already exists in the destination folder.");
                return false;
            }

            if (ec)
            {
                editorState.AssetOperationFeedback.Set(Editor::EditorFeedbackSeverity::Error,
                    "Move failed",
                    "Failed to inspect destination '" + destinationPath.generic_string() + "': " + ec.message());
                return false;
            }

            const std::string destinationKey = destinationPath.generic_string();
            if (!plannedDestinationPaths.insert(destinationKey).second)
            {
                editorState.AssetOperationFeedback.Set(Editor::EditorFeedbackSeverity::Error,
                    "Move failed",
                    "Multiple selected assets would produce the same destination file '" + filename.string() + "'.");
                return false;
            }

            std::string conflictingAssetID;
            if (IsDestinationRegisteredByAnotherAsset(projectContext, record->ID, destinationPath, conflictingAssetID))
            {
                editorState.AssetOperationFeedback.Set(Editor::EditorFeedbackSeverity::Error,
                    "Move failed",
                    "The destination path is already registered as asset '" + conflictingAssetID + "'.");

                return false;
            }

            plan.push_back(PlannedAssetMove
                {
                    record->ID,
                    GetAssetBrowserFolderForRecord(projectContext, *record),
                    destinationPath
                }
            );
        }

        std::vector<PlannedAssetMove> completedMoves;

        completedMoves.reserve(plan.size());
        for (const PlannedAssetMove& plannedMove : plan)
        {
            std::filesystem::path movedAssetPath;
            std::string moveErrorMessage;

            if (projectContext.MoveAssetToFolder(plannedMove.AssetID, targetFolder, movedAssetPath, moveErrorMessage))
            {
                completedMoves.push_back(plannedMove);
                continue;
            }

            std::vector<std::string> rollbackFailures;

            for (auto rollbackIterator = completedMoves.rbegin(); rollbackIterator != completedMoves.rend(); ++rollbackIterator)
            {
                std::filesystem::path restoredAssetPath;
                std::string rollbackError;

                if (!projectContext.MoveAssetToFolder(rollbackIterator->AssetID, rollbackIterator->SourceFolder, restoredAssetPath, rollbackError))
                    rollbackFailures.push_back(rollbackIterator->AssetID + ": " + (rollbackError.empty()? "unknown rollback failure" : rollbackError));
            }

            editorState.QueueRequest(Editor::EditorRequestType::ReloadAssets);
            std::string feedbackMessage = moveErrorMessage.empty() ? "The selected assets could not be moved." : moveErrorMessage;

            if (rollbackFailures.empty() && !completedMoves.empty())
            {
                feedbackMessage += " Previously moved assets were restored.";
            }
            else if (!rollbackFailures.empty())
            {
                feedbackMessage += " Some rollback operations also failed:";
                for (const std::string& rollbackFailure : rollbackFailures)
                {
                    feedbackMessage += "\n• " + rollbackFailure;
                }
            }

            editorState.AssetOperationFeedback.Set(Editor::EditorFeedbackSeverity::Error,
                "Move failed",
                feedbackMessage);
            return false;
        }

        editorState.QueueRequest(Editor::EditorRequestType::ReloadAssets);

        ClearAssetSelection(state);
        state.SelectedContentFolder.clear();

        editorState.AssetOperationFeedback.Set(Editor::EditorFeedbackSeverity::Success,
            plan.size() == 1 ? "Asset moved" : "Assets moved",
            "Moved " + MakeMoveCountLabel(plan.size()) + " to '" + targetFolder + "'.");

        return true;
    }

    bool ExecuteAssetDuplicateOperation(
        Project::ProjectContext &projectContext,
        Editor::EditorState &editorState,
        AssetBrowserState &state,
        const AssetBrowserOperationRequest &request)
    {
        if (!request.IsValid() || request.Action != AssetBrowserOperationAction::Duplicate) return false;
        if (editorState.Scene.Mode != Editor::SceneMode::Edit)
        {
            editorState.AssetOperationFeedback.Set(Editor::EditorFeedbackSeverity::Warning,
                "Duplicate rejected",
                "Assets cannot be duplicated while runtime mode is active.");
            return false;
        }

        const std::vector<std::string> assetIDs = NormalizeAssetIDs(request.AssetIDs);
        if (assetIDs.empty()) return false;

        const std::string targetFolder = NormalizeProjectRelativePathString(request.TargetFolder);
        if (!DoesAssetFolderExist(projectContext, targetFolder))
        {
            editorState.AssetOperationFeedback.Set(Editor::EditorFeedbackSeverity::Error,
                "Duplicate failed",
                "The destination folder no longer exists.");
            return false;
        }

        const std::filesystem::path resolvedTargetFolder = ResolveAssetFolderPath(projectContext, targetFolder).lexically_normal();
        std::vector<PlannedAssetDuplicate> plan;
        plan.reserve(assetIDs.size());

        std::unordered_set<std::string> plannedAssetIDs;
        std::unordered_set<std::string> plannedDestinationPaths;

        for (const std::string& assetID : assetIDs)
        {
            const Project::AssetRecord* record = Project::AssetRegistryIO::FindByID(projectContext.GetAssetRegistry(), assetID);
            if (!record)
            {
                editorState.AssetOperationFeedback.Set(Editor::EditorFeedbackSeverity::Error,
                    "Duplicate failed",
                    "Asset '" + assetID + "' is no longer registered.");
                return false;
            }
            if (record->BuiltIn)
            {
                editorState.AssetOperationFeedback.Set(Editor::EditorFeedbackSeverity::Error,
                    "Duplicate failed",
                    "Built-in asset '" + record->ID + "' has no physical source file to duplicate.");
                return false;
            }
            const AssetPathDiagnostics diagnostics = InspectAssetPath(projectContext, *record);
            if (diagnostics.IsMissing())
            {
                editorState.AssetOperationFeedback.Set(Editor::EditorFeedbackSeverity::Error,
                    "Duplicate failed",
                    "Asset '" + record->ID + "' does not have a valid physical source file.");
                return false;
            }

            const std::filesystem::path sourcePath = diagnostics.ResolvedPath.lexically_normal();
            const std::filesystem::path sourceFilename = sourcePath.filename();
            if (sourceFilename.empty())
            {
                editorState.AssetOperationFeedback.Set(Editor::EditorFeedbackSeverity::Error,
                    "Duplicate failed",
                    "Asset '" + record->ID + "' has no valid source filename.");
                return false;
            }

            const std::string newAssetID = MakeUniqueDuplicateAssetID(projectContext, record->ID, plannedAssetIDs);
            if (newAssetID.empty())
            {
                editorState.AssetOperationFeedback.Set(Editor::EditorFeedbackSeverity::Error,
                    "Duplicate failed",
                    "Quiet could not generate a unique asset ID for '" + record->ID + "'.");
                return false;
            }

            std::filesystem::path destinationPath;
            std::string pathErrorMessage;

            if (!TryMakeUniqueDuplicatePath(projectContext, resolvedTargetFolder, sourceFilename, plannedDestinationPaths, destinationPath, pathErrorMessage))
            {
                editorState.AssetOperationFeedback.Set(Editor::EditorFeedbackSeverity::Error,
                    "Duplicate failed",
                    pathErrorMessage);
                return false;
            }

            plannedAssetIDs.insert(newAssetID);
            plannedDestinationPaths.insert(destinationPath.generic_string());

            plan.push_back(PlannedAssetDuplicate
                {
                    newAssetID,
                    record->Type,
                    sourcePath,
                    destinationPath
                }
            );
        }

        std::vector<CompletedAssetDuplicate> completedDuplicates;
        completedDuplicates.reserve(plan.size());

        for (const PlannedAssetDuplicate& duplicate : plan)
        {
            std::error_code copyError;

            const bool copied = std::filesystem::copy_file(
                    duplicate.SourcePath,
                    duplicate.DestinationPath,
                    std::filesystem::copy_options::none,
                    copyError);

            if (!copied || copyError)
            {
                std::string feedbackMessage = "Failed to copy '" + duplicate.SourcePath.generic_string() + "'";
                if (copyError)
                {
                    feedbackMessage += ": " + copyError.message();
                }

                for (auto iterator = completedDuplicates.rbegin(); iterator != completedDuplicates.rend(); ++iterator)
                {
                    if (!projectContext.RemoveAssetByID(iterator->AssetID))
                    {
                        feedbackMessage += "\n• Failed to remove rolled-back registry entry '" + iterator->AssetID + "'.";
                        continue;
                    }

                    std::error_code removeError;
                    std::filesystem::remove(iterator->Path, removeError);
                    if (removeError)
                    {
                        feedbackMessage += "\n• Failed to remove rolled-back file '" + iterator->Path.generic_string() + "': " + removeError.message();
                    }
                }

                editorState.QueueRequest(Editor::EditorRequestType::ReloadAssets);
                editorState.AssetOperationFeedback.Set(Editor::EditorFeedbackSeverity::Error,
                    "Duplicate failed",
                    feedbackMessage);
                return false;
            }

            bool registered = false;
            switch (duplicate.Type)
            {
                case Project::AssetType::Mesh:
                    registered = projectContext.RegisterMeshAsset(duplicate.NewAssetID, duplicate.DestinationPath, false);
                    break;

                case Project::AssetType::Texture:
                    registered = projectContext.RegisterTextureAsset(duplicate.NewAssetID, duplicate.DestinationPath, false);
                    break;

                case Project::AssetType::Unknown:
                default:
                    registered = false;
                    break;
            }

            if (!registered)
            {
                std::string feedbackMessage = "The copied file could not be registered as asset '" + duplicate.NewAssetID + "'.";
                std::error_code currentRemoveError;
                std::filesystem::remove(duplicate.DestinationPath, currentRemoveError);

                if (currentRemoveError)
                {
                    feedbackMessage += "\n• Failed to remove the unregistered copied file '" +
                        duplicate.DestinationPath.generic_string() + "': " + currentRemoveError.message();
                }

                for (auto iterator = completedDuplicates.rbegin(); iterator != completedDuplicates.rend(); ++iterator)
                {
                    if (!projectContext.RemoveAssetByID(iterator->AssetID))
                    {
                        feedbackMessage += "\n• Failed to remove rolled-back registry entry '" + iterator->AssetID + "'.";
                        continue;
                    }

                    std::error_code rollbackRemoveError;
                    std::filesystem::remove(iterator->Path, rollbackRemoveError);
                    if (rollbackRemoveError)
                    {
                        feedbackMessage += "\n• Failed to remove rolled-back file '" + iterator->Path.generic_string() + "': " + rollbackRemoveError.message();
                    }
                }

                editorState.QueueRequest(Editor::EditorRequestType::ReloadAssets);
                editorState.AssetOperationFeedback.Set(Editor::EditorFeedbackSeverity::Error,
                    "Duplicate failed",
                    feedbackMessage);
                return false;
            }

            completedDuplicates.push_back(
                CompletedAssetDuplicate{
                    duplicate.NewAssetID,
                    duplicate.DestinationPath
                }
            );
        }

        state.SelectedAssetIDs.clear();
        state.SelectedAssetIDs.reserve(plan.size());

        for (const PlannedAssetDuplicate& duplicate : plan)
        {
            state.SelectedAssetIDs.push_back(duplicate.NewAssetID);
        }

        state.SelectedDetailsAssetID = state.SelectedAssetIDs.empty() ? std::string{} : state.SelectedAssetIDs.front();
        state.SelectionAnchorAssetID = state.SelectedDetailsAssetID;
        state.SelectedContentFolder.clear();

        editorState.QueueRequest(Editor::EditorRequestType::ReloadAssets);
        editorState.AssetOperationFeedback.Set(Editor::EditorFeedbackSeverity::Success,
            plan.size() == 1 ? "Asset duplicated" : "Assets duplicated",
            "Duplicated " + MakeDuplicateCountLabel(plan.size()) + " into '" + targetFolder + "'.");
        return true;
    }

    bool ExecuteAssetRenameOperation(
        Project::ProjectContext &projectContext,
        Editor::EditorState &editorState,
        AssetBrowserState &state,
        const AssetBrowserOperationRequest &request)
    {
        if (!request.IsValid() || request.Action != AssetBrowserOperationAction::Rename) return false;
        if (editorState.Scene.Mode != Editor::SceneMode::Edit)
        {
            state.AssetRenameErrorMessage = "Assets cannot be renamed while runtime mode is active.";
            editorState.AssetOperationFeedback.Set(Editor::EditorFeedbackSeverity::Warning,
                "Rename rejected",
                state.AssetRenameErrorMessage);
            return false;
        }

        const std::string& assetID = request.AssetIDs.front();
        std::filesystem::path renamedAssetPath;
        std::string renameErrorMessage;

        if (!projectContext.RenameAssetFile(assetID, request.NewName, renamedAssetPath, renameErrorMessage))
        {
            state.AssetRenameErrorMessage = renameErrorMessage.empty() ? "The selected asset could not be renamed." : renameErrorMessage;
            editorState.AssetOperationFeedback.Set(Editor::EditorFeedbackSeverity::Error,
                "Rename failed",
                state.AssetRenameErrorMessage);
            return false;
        }

        state.AssetRenameErrorMessage.clear();

        editorState.QueueRequest(Editor::EditorRequestType::ReloadAssets);
        editorState.AssetOperationFeedback.Set(Editor::EditorFeedbackSeverity::Success,
            "Asset renamed",
            "Renamed the source file for asset '" + assetID + "' to '" + renamedAssetPath.generic_string() + "'. The stable asset ID was preserved.");
        return true;
    }

    bool ExecuteAssetRemoveFromRegistryOperation(
        Project::ProjectContext &projectContext,
        Editor::EditorState &editorState,
        AssetBrowserState &state,
        const Scene::Scene &scene,
        const AssetBrowserOperationRequest &request)
    {
        if (!request.IsValid() || request.Action != AssetBrowserOperationAction::RemoveFromRegistry) return false;
        if (editorState.Scene.Mode != Editor::SceneMode::Edit)
        {
            state.RemoveErrorMessage = "Assets cannot be removed while runtime mode is active.";
            editorState.AssetOperationFeedback.Set(Editor::EditorFeedbackSeverity::Warning,
                "Remove rejected",
                state.RemoveErrorMessage);
            return false;
        }

        const std::vector<std::string> assetIDs = NormalizeAssetIDs(request.AssetIDs);
        if (assetIDs.empty()) return false;
        for (const std::string& assetID : assetIDs)
        {
            const Project::AssetRecord* record = Project::AssetRegistryIO::FindByID(projectContext.GetAssetRegistry(), assetID);
            if (!record)
            {
                state.RemoveErrorMessage = "Asset '" + assetID + "' is no longer registered.";
                editorState.AssetOperationFeedback.Set(Editor::EditorFeedbackSeverity::Error,
                    "Remove failed",
                    state.RemoveErrorMessage);
                return false;
            }

            if (record->BuiltIn)
            {
                state.RemoveErrorMessage = "Built-in asset '" + record->ID + "' cannot be removed.";
                editorState.AssetOperationFeedback.Set(Editor::EditorFeedbackSeverity::Error,
                    "Remove failed",
                    state.RemoveErrorMessage);
                return false;
            }

            const std::uint32_t referenceCount = CountAssetReferences(scene, *record);
            if (referenceCount > 0)
            {
                state.RemoveErrorMessage = "Asset '" + record->ID + "' is referenced by " + std::to_string(referenceCount)
                    + (referenceCount == 1 ? " actor" : " actors") + " in the current scene.";

                editorState.AssetOperationFeedback.Set(Editor::EditorFeedbackSeverity::Error,
                    "Remove failed",
                    state.RemoveErrorMessage);
                return false;
            }
        }

        std::string removeErrorMessage;

        if (!projectContext.RemoveAssetsByID(assetIDs, removeErrorMessage))
        {
            state.RemoveErrorMessage = removeErrorMessage.empty() ? "The selected assets could not be removed from the registry." : removeErrorMessage;
            editorState.AssetOperationFeedback.Set(Editor::EditorFeedbackSeverity::Error,
                "Remove failed",
                state.RemoveErrorMessage);
            return false;
        }

        for (const std::string& assetID : assetIDs)
        {
            RemoveAssetFromSelection(state, assetID);
        }

        SynchronizeAssetClipboard(projectContext, state);

        state.SelectedContentFolder.clear();
        state.RemoveErrorMessage.clear();

        editorState.QueueRequest(Editor::EditorRequestType::ReloadAssets);
        editorState.AssetOperationFeedback.Set(Editor::EditorFeedbackSeverity::Success,
            assetIDs.size() == 1 ? "Asset removed" : "Assets removed",
            "Removed " + MakeMoveCountLabel(assetIDs.size()) + " from the registry. Source files were not deleted.");
        return true;
    }

    AssetFolderDeleteSummary InspectAssetFolderForDeletion(
        const Project::ProjectContext& projectContext,
        const Scene::Scene& scene,
        const std::string& folder)
    {
        AssetFolderDeleteSummary summary{};
        summary.Folder = NormalizeProjectRelativePathString(folder);

        const std::string assetsRoot = GetAssetsRootRelativePath(projectContext);
        if (summary.Folder.empty() || summary.Folder == assetsRoot)
        {
            summary.ErrorMessage = "The project Assets root cannot be deleted.";
            return summary;
        }

        if (!DoesAssetFolderExist(projectContext, summary.Folder))
        {
            summary.ErrorMessage = "The selected folder no longer exists.";
            return summary;
        }

        std::error_code ec;
        const std::filesystem::path resolvedFolder = NormalizeOperationPath(ResolveAssetFolderPath(projectContext, summary.Folder));
        std::unordered_set<std::string> registeredPhysicalPaths;

        for (const Project::AssetRecord& record : projectContext.GetAssetRegistry().Records)
        {
            if (record.BuiltIn || record.Path.empty()) continue;

            const std::filesystem::path assetPath = NormalizeOperationPath(ResolveDisplayPath(projectContext, record.Path));
            if (!IsOperationPathInsideOrEqual(assetPath, resolvedFolder)) continue;

            summary.RegisteredAssetIDs.push_back(record.ID);

            ec.clear();
            const bool sourceExists = std::filesystem::exists(assetPath, ec);
            if (ec)
            {
                summary.ErrorMessage = "Failed to inspect registered asset '" + record.ID + "': " + ec.message();
                return summary;
            }

            if (!sourceExists)
            {
                ++summary.MissingRegisteredAssetCount;
            }
            else
            {
                ec.clear();
                if (std::filesystem::is_regular_file(assetPath, ec) && !ec)
                {
                    registeredPhysicalPaths.insert(assetPath.generic_string());
                }
            }

            const std::uint32_t referenceCount = CountAssetReferences(scene, record);
            if (referenceCount > 0)
            {
                ++summary.ReferencedAssetCount;
                summary.SceneReferenceCount += referenceCount;
            }
        }

        std::sort(summary.RegisteredAssetIDs.begin(), summary.RegisteredAssetIDs.end());
        for (std::filesystem::recursive_directory_iterator iterator(resolvedFolder, std::filesystem::directory_options::none, ec), end;
            iterator != end; iterator.increment(ec))
        {
            if (ec)
            {
                summary.ErrorMessage = "Failed to inspect the folder contents: " + ec.message();
                return summary;
            }

            const std::filesystem::file_status status = iterator->symlink_status(ec);
            if (ec)
            {
                summary.ErrorMessage = "Failed to inspect '" + iterator->path().generic_string() + "': " + ec.message();
                return summary;
            }

            if (std::filesystem::is_symlink(status))
            {
                ++summary.SymbolicLinkCount;
                continue;
            }

            if (std::filesystem::is_directory(status))
            {
                ++summary.NestedFolderCount;
                continue;
            }

            if (std::filesystem::is_regular_file(status))
            {
                ++summary.PhysicalFileCount;
                const std::filesystem::path filePath = NormalizeOperationPath(iterator->path());

                if (!registeredPhysicalPaths.contains(filePath.generic_string()))
                {
                    ++summary.UnregisteredFileCount;
                }
                continue;
            }
            ++summary.UnsupportedEntryCount;
        }

        if (ec)
        {
            summary.ErrorMessage = "Failed to inspect the folder contents: " + ec.message();
        }
        return summary;
    }

    bool ExecuteAssetFolderDeleteOperation(
        Project::ProjectContext& projectContext,
        Editor::EditorState& editorState,
        AssetBrowserState& state,
        const Scene::Scene& scene,
        const std::string& folder)
    {
        if (editorState.Scene.Mode != Editor::SceneMode::Edit)
        {
            state.FolderOperationErrorMessage = "Folders cannot be deleted while runtime mode is active.";
            editorState.AssetOperationFeedback.Set(Editor::EditorFeedbackSeverity::Warning,
                "Folder deletion rejected",
                state.FolderOperationErrorMessage);
            return false;
        }

        const AssetFolderDeleteSummary summary = InspectAssetFolderForDeletion(projectContext, scene, folder);
        if (!summary.IsValid())
        {
            state.FolderOperationErrorMessage = summary.ErrorMessage.empty()
                ? "The folder could not be inspected."
                : summary.ErrorMessage;

            editorState.AssetOperationFeedback.Set(Editor::EditorFeedbackSeverity::Error,
                "Folder deletion failed",
                state.FolderOperationErrorMessage);
            return false;
        }

        if (summary.ReferencedAssetCount > 0)
        {
            state.FolderOperationErrorMessage = std::to_string(summary.ReferencedAssetCount) +
                (summary.ReferencedAssetCount == 1 ? " registered asset is" : " registered assets are") +
                " referenced by the active scene.";

            editorState.AssetOperationFeedback.Set(Editor::EditorFeedbackSeverity::Error,
                "Folder deletion blocked",
                state.FolderOperationErrorMessage);
            return false;
        }

        if (summary.SymbolicLinkCount > 0)
        {
            state.FolderOperationErrorMessage = "The folder contains symbolic links and cannot be deleted safely.";
            editorState.AssetOperationFeedback.Set(Editor::EditorFeedbackSeverity::Error,
                "Folder deletion blocked",
                state.FolderOperationErrorMessage);
            return false;
        }

        if (summary.UnsupportedEntryCount > 0)
        {
            state.FolderOperationErrorMessage = "The folder contains unsupported filesystem entries.";
            editorState.AssetOperationFeedback.Set(Editor::EditorFeedbackSeverity::Error,
                "Folder deletion blocked",
                state.FolderOperationErrorMessage);
            return false;
        }

        const std::string deletedFolder = summary.Folder;
        const std::string parentFolder = GetParentAssetFolder(projectContext, deletedFolder);
        std::vector<std::string> removedAssetIDs;

        std::string deleteErrorMessage;
        std::string deleteWarningMessage;

        if (!projectContext.DeleteAssetFolderFromProject(deletedFolder, removedAssetIDs, deleteErrorMessage, deleteWarningMessage))
        {
            state.FolderOperationErrorMessage = deleteErrorMessage.empty()
                ? "The folder could not be deleted."
                : deleteErrorMessage;

            editorState.AssetOperationFeedback.Set(Editor::EditorFeedbackSeverity::Error,
                "Folder deletion failed",
                state.FolderOperationErrorMessage);
            return false;
        }

        for (const std::string& assetID : removedAssetIDs)
        {
            RemoveAssetFromSelection(state, assetID);
        }

        SynchronizeAssetClipboard(projectContext, state);

        RepairAssetFolderHistoryAfterDelete(state, deletedFolder, parentFolder);

        state.SelectedContentFolder.clear();
        state.FolderOperationErrorMessage.clear();
        editorState.QueueRequest(Editor::EditorRequestType::ReloadAssets);

        const std::string resultMessage = "Deleted folder '" + deletedFolder + "' with " +
            std::to_string(summary.PhysicalFileCount) + (summary.PhysicalFileCount == 1 ? " file, " : " files, ") +
            std::to_string(summary.NestedFolderCount) + (summary.NestedFolderCount == 1 ? " nested folder, and " : " nested folders, and ") +
            std::to_string(removedAssetIDs.size()) + (removedAssetIDs.size() == 1 ? " registry record." : " registry records.");

        if (!deleteWarningMessage.empty())
        {
            editorState.AssetOperationFeedback.Set(Editor::EditorFeedbackSeverity::Warning,
                "Folder deleted with warning",
                resultMessage + "\n\n" + deleteWarningMessage);
        }
        else
        {
            editorState.AssetOperationFeedback.Set(Editor::EditorFeedbackSeverity::Success,
                "Folder deleted",
                resultMessage);
        }
        return true;
    }

    bool ExecuteAssetDeleteFromProjectOperation(
        Project::ProjectContext &projectContext,
        Editor::EditorState &editorState,
        AssetBrowserState &state,
        const Scene::Scene &scene,
        const AssetBrowserOperationRequest &request)
    {
        if (!request.IsValid() || request.Action != AssetBrowserOperationAction::DeleteFromProject) return false;

        if (editorState.Scene.Mode != Editor::SceneMode::Edit)
        {
            state.DeleteErrorMessage = "Assets cannot be deleted while runtime mode is active.";
            editorState.AssetOperationFeedback.Set(Editor::EditorFeedbackSeverity::Warning,
                "Delete rejected",
                state.DeleteErrorMessage);
            return false;
        }

        const std::vector<std::string> assetIDs = NormalizeAssetIDs(request.AssetIDs);
        if (assetIDs.empty()) return false;
        for (const std::string& assetID : assetIDs)
        {
            const Project::AssetRecord* record = Project::AssetRegistryIO::FindByID(projectContext.GetAssetRegistry(), assetID);
            if (!record)
            {
                state.DeleteErrorMessage = "Asset '" + assetID + "' is no longer registered.";
                editorState.AssetOperationFeedback.Set(Editor::EditorFeedbackSeverity::Error,
                    "Delete failed",
                    state.DeleteErrorMessage);
                return false;
            }

            if (record->BuiltIn)
            {
                state.DeleteErrorMessage = "Built-in asset '" + record->ID + "' cannot be deleted.";
                editorState.AssetOperationFeedback.Set(Editor::EditorFeedbackSeverity::Error,
                    "Delete failed",
                    state.DeleteErrorMessage);
                return false;
            }

            const AssetPathDiagnostics diagnostics = InspectAssetPath(projectContext, *record);
            if (diagnostics.IsMissing())
            {
                state.DeleteErrorMessage = "Asset '" + record->ID + "' does not have a valid physical source file. Use Remove from Registry instead.";
                editorState.AssetOperationFeedback.Set(Editor::EditorFeedbackSeverity::Error,
                    "Delete failed",
                    state.DeleteErrorMessage);
                return false;
            }

            if (diagnostics.External)
            {
                state.DeleteErrorMessage = "External asset '" + record->ID + "' cannot be physically deleted through the project Assets browser. Use Remove from Registry instead.";
                editorState.AssetOperationFeedback.Set(Editor::EditorFeedbackSeverity::Error,
                    "Delete failed",
                    state.DeleteErrorMessage);
                return false;
            }

            const std::uint32_t referenceCount = CountAssetReferences(scene, *record);
            if (referenceCount > 0)
            {
                state.DeleteErrorMessage = "Asset '" + record->ID + "' is referenced by " +
                    std::to_string(referenceCount) + (referenceCount == 1 ? " actor" : " actors") + " in the current scene.";

                editorState.AssetOperationFeedback.Set(Editor::EditorFeedbackSeverity::Error,
                    "Delete failed",
                    state.DeleteErrorMessage);
                return false;
            }
        }

        std::string deleteErrorMessage;
        std::string deleteWarningMessage;

        if (!projectContext.DeleteAssetsFromProject(assetIDs, deleteErrorMessage, deleteWarningMessage))
        {
            state.DeleteErrorMessage = deleteErrorMessage.empty() ? "The selected assets could not be deleted." : deleteErrorMessage;
            editorState.AssetOperationFeedback.Set(Editor::EditorFeedbackSeverity::Error,
                "Delete failed",
                state.DeleteErrorMessage);
            return false;
        }

        for (const std::string& assetID : assetIDs)
        {
            RemoveAssetFromSelection(state, assetID);
        }

        SynchronizeAssetClipboard(projectContext, state);

        state.SelectedContentFolder.clear();
        state.DeleteErrorMessage.clear();

        editorState.QueueRequest(Editor::EditorRequestType::ReloadAssets);
        if (!deleteWarningMessage.empty())
        {
            editorState.AssetOperationFeedback.Set(Editor::EditorFeedbackSeverity::Warning,
                assetIDs.size() == 1 ? "Asset deleted with warning" : "Assets deleted with warning",
                deleteWarningMessage);
        }
        else
        {
            editorState.AssetOperationFeedback.Set(Editor::EditorFeedbackSeverity::Success,
                assetIDs.size() == 1 ? "Asset deleted" : "Assets deleted",
                "Permanently deleted " + MakeMoveCountLabel(assetIDs.size()) + " from the project.");
        }
        return true;
    }

    bool IsAssetClipboardAvailable(
        const Project::ProjectContext& projectContext,
        const AssetBrowserState& state)
    {
        return state.Clipboard.IsValid() && state.Clipboard.ProjectRoot == MakeAssetClipboardProjectKey(projectContext);
    }

    void SynchronizeAssetClipboard(
        const Project::ProjectContext& projectContext,
        AssetBrowserState& state)
    {
        if (!state.Clipboard.IsValid())
        {
            state.Clipboard.Clear();
            return;
        }

        if (state.Clipboard.ProjectRoot != MakeAssetClipboardProjectKey(projectContext))
        {
            state.Clipboard.Clear();
            return;
        }

        state.Clipboard.AssetIDs.erase(
            std::remove_if(
                state.Clipboard.AssetIDs.begin(),
                state.Clipboard.AssetIDs.end(),
                [&](const std::string& assetID)
                {
                    return Project::AssetRegistryIO::FindByID(projectContext.GetAssetRegistry(), assetID) == nullptr;
                }
            ),
            state.Clipboard.AssetIDs.end()
        );

        if (state.Clipboard.AssetIDs.empty())
            state.Clipboard.Clear();
    }

    bool ExecuteAssetBrowserOperation(
        Project::ProjectContext& projectContext,
        Editor::EditorState& editorState,
        AssetBrowserState& state,
        const AssetBrowserOperationRequest& request,
        const Scene::Scene* scene)
    {
        if (!request.IsValid()) return false;

        switch (request.Action)
        {
            case AssetBrowserOperationAction::CopySelection:
                return StoreAssetClipboard(projectContext, editorState, state, AssetBrowserClipboardMode::Copy,request.AssetIDs);

            case AssetBrowserOperationAction::CutSelection:
                return StoreAssetClipboard(projectContext, editorState, state, AssetBrowserClipboardMode::Move, request.AssetIDs);

            case AssetBrowserOperationAction::Duplicate:
                return ExecuteAssetDuplicateOperation(projectContext, editorState, state, request);

            case AssetBrowserOperationAction::Rename:
                return ExecuteAssetRenameOperation(projectContext, editorState, state, request);

            case AssetBrowserOperationAction::RemoveFromRegistry:
            {
                if (!scene)
                {
                    state.RemoveErrorMessage = "The active scene is required to validate asset references.";
                    editorState.AssetOperationFeedback.Set(Editor::EditorFeedbackSeverity::Error,
                        "Remove failed",
                        state.RemoveErrorMessage);
                    return false;
                }

                return ExecuteAssetRemoveFromRegistryOperation(projectContext, editorState, state, *scene, request);
            }

            case AssetBrowserOperationAction::DeleteFromProject:
            {
                if (!scene)
                {
                    state.DeleteErrorMessage = "The active scene is required to validate asset references.";
                    editorState.AssetOperationFeedback.Set(Editor::EditorFeedbackSeverity::Error,
                        "Delete failed",
                        state.DeleteErrorMessage);
                    return false;
                }

                return ExecuteAssetDeleteFromProjectOperation(projectContext, editorState, state, *scene, request);
            }

            case AssetBrowserOperationAction::Paste:
            {
                SynchronizeAssetClipboard(projectContext, state);
                if (!IsAssetClipboardAvailable(projectContext, state))
                {
                    editorState.AssetOperationFeedback.Set(Editor::EditorFeedbackSeverity::Warning,
                        "Paste unavailable",
                        "The Asset Browser clipboard is empty.");
                    return false;
                }

                if (state.Clipboard.Mode == AssetBrowserClipboardMode::Copy)
                {
                    AssetBrowserOperationRequest duplicateRequest{};
                    duplicateRequest.Action = AssetBrowserOperationAction::Duplicate;
                    duplicateRequest.AssetIDs = state.Clipboard.AssetIDs;
                    duplicateRequest.TargetFolder = request.TargetFolder;

                    // Copy remains in the clipboard so it can be pasted into more than one destination.
                    return ExecuteAssetDuplicateOperation(projectContext, editorState, state, duplicateRequest);
                }

                if (state.Clipboard.Mode == AssetBrowserClipboardMode::Move)
                {
                    AssetBrowserAssetMoveRequest moveRequest{};
                    moveRequest.AssetIDs = state.Clipboard.AssetIDs;
                    moveRequest.TargetFolder = request.TargetFolder;

                    const bool moved = ExecuteAssetMoveOperation(projectContext, editorState, state, moveRequest);
                    if (moved) state.Clipboard.Clear();
                    return moved;
                }
                return false;
            }

            case AssetBrowserOperationAction::None:
            default:
                return false;
        }
    }
}

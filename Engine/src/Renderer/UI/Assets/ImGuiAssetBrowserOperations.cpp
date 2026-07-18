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

    std::string MakeMoveCountLabel(const std::size_t count)
    {
        if (count == 1) return "1 asset";
        return std::to_string(count) + " assets";
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
}
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

namespace Quiet::Editor
{
    struct EditorState;
}

namespace Quiet::Audio
{
    class AudioLibrary;
}

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
    bool MatchesAssetSource(const Project::AssetRecord& record, AssetBrowserSource source);
    bool MatchesAssetFolder(const Project::ProjectContext& projectContext, const Project::AssetRecord& record, const std::string& currentAssetFolder);

    std::vector<const Project::AssetRecord*> BuildAssetFolderList(const Project::ProjectContext& projectContext,
                                                                  const Project::AssetRegistry& registry,
                                                                  const std::string& currentAssetFolder);
    std::vector<const Project::AssetRecord*> BuildBuiltInAssetList(const Project::AssetRegistry& registry);
    std::vector<const Project::AssetRecord*> BuildVisibleAssetList(const Project::ProjectContext& projectContext,
                                                                   const std::vector<const Project::AssetRecord*>& folderAssets,
                                                                   AssetBrowserTypeFilter typeFilter,
                                                                   AssetBrowserDiagnosticsFilter diagnosticsFilter,
                                                                   const std::string& searchQuery);

    std::size_t CountAssetByType(const Project::AssetRegistry& registry, Project::AssetType type, AssetBrowserSource source);

    // File/path helpers
    std::string NormalizeProjectRelativePathString(const std::filesystem::path& path);
    std::string GetAssetsRootRelativePath(const Project::ProjectContext& projectContext);
    bool IsAssetFolderInsideAssetsRoot(const std::string& candidateFolder, const std::string& assetsRoot);

    std::filesystem::path ResolveAssetFolderPath(const Project::ProjectContext& projectContext, const std::string& assetFolder);
    std::vector<AssetBrowserFolderEntry> BuildAssetChildFolderList(const Project::ProjectContext& projectContext, const std::string& parentFolder);
    std::vector<AssetBrowserFolderEntry> BuildVisibleAssetFolderList(const std::vector<AssetBrowserFolderEntry>& childFolders, const std::string& searchQuery);

    std::string MakeProjectRelativeAssetFolderPath(const Project::ProjectContext& projectContext, const std::filesystem::path& absoluteFolderPath);
    std::string GetAssetBrowserFolderForRecord(const Project::ProjectContext& projectContext, const Project::AssetRecord& record);
    void ValidateCurrentAssetFolder(const Project::ProjectContext& projectContext, std::string& currentAssetFolder);
    std::string GetParentAssetFolder(const Project::ProjectContext& projectContext, const std::string& currentAssetFolder);
    std::vector<std::string> BuildAssetFolderBreadcrumbs(const Project::ProjectContext& projectContext, const std::string& currentAssetFolder);

    bool DoesAssetFolderExist(const Project::ProjectContext& projectContext, const std::string& assetFolder);
    void SynchronizeAssetFolderHistory(const Project::ProjectContext& projectContext, AssetBrowserState& state);

    [[nodiscard]] bool CanNavigateAssetFolderBack(const AssetBrowserState& state);
    [[nodiscard]] bool CanNavigateAssetFolderForward(const AssetBrowserState& state);

    void NavigateToAssetFolder(const Project::ProjectContext& projectContext, AssetBrowserState& state, const std::string& targetFolder);
    void NavigateAssetFolderBack(const Project::ProjectContext& projectContext, AssetBrowserState& state);
    void NavigateAssetFolderForward(const Project::ProjectContext& projectContext, AssetBrowserState& state);

    void RemapAssetFolderHistoryAfterRename(AssetBrowserState& state, const std::string& previousFolder, const std::string& renamedFolder);
    void RepairAssetFolderHistoryAfterDelete(AssetBrowserState& state, const std::string& deletedFolder, const std::string& fallbackFolder);

    // Asset selection
    [[nodiscard]] bool IsAssetSelected(const AssetBrowserState& state, const std::string& assetID);

    void ClearAssetSelection(AssetBrowserState& state);
    void SelectSingleAsset(AssetBrowserState& state, const std::string& assetID);
    void HandleAssetSelectionClick(AssetBrowserState& state,
                                   const std::vector<const Project::AssetRecord*>& orderedAssets,
                                   const std::string& assetID,
                                   bool toggleModifier,
                                   bool rangeModifier);

    void RemoveAssetFromSelection(AssetBrowserState& state, const std::string& assetID);
    void RepairAssetSelection(AssetBrowserState& state, const std::vector<const Project::AssetRecord*>& folderAssets);
    void SetAssetBrowserSource(AssetBrowserState& state, AssetBrowserSource source);

    // Path diagnostics
    struct AssetPathDiagnostics
    {
        std::filesystem::path ResolvedPath{};

        bool BuiltIn = false;
        bool HasPath = false;
        bool Exists = false;
        bool RegularFile = false;
        bool External = false;

        [[nodiscard]] bool IsMissing() const
        {
            return !BuiltIn && (!HasPath || !Exists || !RegularFile);
        }

        [[nodiscard]] bool HasIssue() const
        {
            return IsMissing() || External;
        }
    };

    struct AssetDiagnosticsCounts
    {
        std::size_t Issues = 0;
        std::size_t Missing = 0;
        std::size_t External = 0;
    };

    std::filesystem::path ResolveDisplayPath(const Project::ProjectContext& projectContext, const std::string& assetPath);

    AssetPathDiagnostics InspectAssetPath(const Project::ProjectContext& projectContext, const Project::AssetRecord& record);

    std::string MakeAssetPathDiagnosticLabel(const AssetPathDiagnostics& diagnostics);
    std::string MakeAssetPathDiagnosticDescription(const AssetPathDiagnostics& diagnostics);

    bool MatchesDiagnosticsFilter(const Project::ProjectContext& projectContext, const Project::AssetRecord& record, AssetBrowserDiagnosticsFilter filter);

    AssetDiagnosticsCounts CountAssetDiagnostics(const Project::ProjectContext& projectContext, AssetBrowserSource source);
    bool DoesAssetPathExist(const Project::ProjectContext& projectContext,const std::string& assetPath);

    // Asset display/status helpers
    std::string MakeStatusLabel(const Project::ProjectContext& projectContext,
                                const Project::AssetRecord& record,
                                const Vulkan::MeshLibrary& meshLibrary,
                                const Vulkan::TextureLibrary& textureLibrary,
                                const Audio::AudioLibrary& audioLibrary,
                                const Editor::EditorState& editorState);
    std::string MakeAssetDisplayName(const Project::AssetRecord& record);

    bool IsAssetReadyStatus(const std::string& status);
    bool IsAssetMissingStatus(const std::string& status);

    std::string MakeAssetBrowserStateLabel(const AssetPathDiagnostics& diagnostics, const Project::AssetRecord& record,
        const std::string& status, bool runtimeModeActive, std::uint32_t referenceCount);

    // Scene usage helpers
    std::uint32_t CountAssetReferences(const Scene::Scene& scene, const Project::AssetRecord& record);
}
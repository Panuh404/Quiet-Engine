#pragma once

#include <array>
#include <string>

namespace Quiet::Renderer::UI::Assets
{
    enum class AssetBrowserTypeFilter
    {
        All,
        Meshes,
        Textures
    };

    enum class AssetBrowserViewMode
    {
        List,
        Browser
    };

    enum class AssetBrowserThumbnailSize
    {
        Small,
        Medium,
        Large
    };

    struct AssetBrowserState
    {
        std::array<char, 128> AssetIDBuffer{};
        std::array<char, 1024> AssetPathBuffer{};
        std::array<char, 128> SearchBuffer{};

        std::string ImportErrorMessage{};
        std::string RemoveErrorMessage{};
        std::string SelectedDetailsAssetID{};
        std::string ConfirmRemoveAssetID{};
        std::string CurrentAssetFolder{};

        bool ReopenImportPopupAfterFileDialog = false;

        AssetBrowserTypeFilter TypeFilter = AssetBrowserTypeFilter::All;
        AssetBrowserViewMode ViewMode = AssetBrowserViewMode::Browser;
        AssetBrowserThumbnailSize ThumbnailSize = AssetBrowserThumbnailSize::Small;
    };
}
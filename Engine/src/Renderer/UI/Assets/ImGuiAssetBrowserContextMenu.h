#pragma once

#include <cstdint>
#include <string>

#include "Quiet/Project/AssetRegistry.h"
#include "Quiet/Project/ProjectContext.h"

namespace Quiet::Renderer::UI::Assets
{
    void DrawAssetContextMenu(
        const char* popupID,
        const Project::ProjectContext& projectContext,
        const Project::AssetRecord& record,
        bool runtimeModeActive,
        std::uint32_t referenceCount,
        std::string& selectedDetailsAssetID,
        std::string& pendingDetailsAssetID,
        std::string& pendingReimportAssetID,
        std::string& pendingRemoveAssetID
    );
}
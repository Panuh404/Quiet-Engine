#pragma once

#include <filesystem>
#include <vector>

#include "Core/CookCache.h"
#include "Core/CookContext.h"

#include "Quiet/Project/AssetRegistry.h"
#include "Quiet/Project/RuntimeManifest.h"

namespace Quiet::Cooker
{
    /**
     * @brief Reuses or converts registered project meshes into validated QMesh files.
     * @ingroup quiet_cooker
     * @return False when any processed mesh reports a required-operation failure.
     * @note Skips built-in and non-Mesh records; continues after per-asset failures.
     * @note Cache hits require matching metadata, output hash, and decoded validity.
     * @note Successful outputs append runtime records and eligible cache entries.
     * @note Output hashing failure prevents caching without failing conversion.
     * @note Does not roll back filesystem writes on failure.
     */
    bool CookProjectMeshes(CookContext& context);
}
#pragma once

#include <filesystem>
#include <vector>

#include "Core/CookCache.h"
#include "Core/CookContext.h"

#include "Quiet/Project/AssetRegistry.h"
#include "Quiet/Project/RuntimeManifest.h"

namespace Quiet::Cooker
{
    bool CookProjectMeshes(CookContext& context);
}
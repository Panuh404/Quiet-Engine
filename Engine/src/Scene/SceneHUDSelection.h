#pragma once

#include "Quiet/Core/UUID.h"

namespace Quiet::Scene
{
    struct SceneHUDSelection
    {
        Core::UUID DocumentAssetID{};
        Core::UUID StyleSheetAssetID{};

        bool operator==(const SceneHUDSelection&) const = default;
    };
}
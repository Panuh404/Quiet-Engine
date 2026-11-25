#pragma once

#include <string>
#include "glm/vec4.hpp"

#include "Quiet/Core/UUID.h"

namespace Quiet::Scene
{
    struct MaterialComponent
    {
        glm::vec4 BaseColor{ 1.0f, 1.0f, 1.0f, 1.0f };

        // Empty means the material uses only BaseColor.
        Core::UUID BaseColorTextureAssetID{};

        [[nodiscard]] bool HasBaseColorTexture() const
        {
            return BaseColorTextureAssetID.IsValid();
        }
    };
}
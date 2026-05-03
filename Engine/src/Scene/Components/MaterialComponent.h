#pragma once

#include "glm/vec4.hpp"

namespace Quiet::Scene
{
    struct MaterialComponent
    {
        glm::vec4 BaseColor{ 1.0f, 1.0f, 1.0f, 1.0f };

        // Empty means the material uses only BaseColor.
        std::string BaseColorTextureAssetID;

        [[nodiscard]] bool HasBaseColorTexture() const
        {
            return !BaseColorTextureAssetID.empty();
        }
    };
}
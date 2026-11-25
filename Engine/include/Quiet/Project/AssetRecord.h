#pragma once

#include <string>
#include <map>

#include "AssetType.h"

#include "Quiet/Core/UUID.h"

namespace Quiet::Project
{
    /**
     * @brief Describes an asset's identity, classification, source path, and origin.
     * @ingroup quiet_assets
     *
     * A record does not establish that its file exists or its resource is loaded.
     */
    struct AssetRecord
    {
        Core::UUID ID{};
        std::string Name;
        AssetType Type = AssetType::Unknown;
        std::string Path;
        bool BuiltIn = false;
        std::map<std::string, Core::UUID> MeshTextureBindings{};

        /**
         * @brief Checks for a valid UUID, a nonempty name, and a known type.
         * @note Does not validate the path, file contents, or loaded resource.
         */
        [[nodiscard]] bool IsValid() const
        {
            return ID.IsValid() && !Name.empty() && Type != AssetType::Unknown;
        }
    };
}

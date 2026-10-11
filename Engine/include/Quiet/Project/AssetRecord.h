#pragma once

#include <string>

#include "AssetType.h"

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
        std::string ID;
        AssetType Type = AssetType::Unknown;
        std::string Path;
        bool BuiltIn = false;

        /**
         * @brief Checks for a nonempty ID and a type other than Unknown.
         * @note Does not validate the path, file contents, or loaded resource.
         */
        [[nodiscard]] bool IsValid() const
        {
            return !ID.empty() && Type != AssetType::Unknown;
        }
    };
}

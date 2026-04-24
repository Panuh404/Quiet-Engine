#pragma once

#include <string>

#include "AssetType.h"

namespace Quiet::Project
{
    struct AssetRecord
    {
        std::string ID;
        AssetType Type = AssetType::Unknown;
        std::string Path;
        bool BuiltIn = false;

        [[nodiscard]] bool IsValid() const
        {
            return !ID.empty() && Type != AssetType::Unknown;
        }
    };
}

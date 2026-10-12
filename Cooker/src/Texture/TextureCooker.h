#pragma once

#include <string>

#include "Core/CookContext.h"

namespace Quiet::Cooker
{
    /**
     * @brief Checks BC7 option creation and synthetic block encode/decode operations.
     * @ingroup quiet_cooker
     * @return False when a checked codec operation reports failure.
     * @note Uses quality 0.5 without measuring reconstruction error.
     */
    bool ValidateCompressonatorBC7(std::string& outError);

    /**
     * @brief Reuses or converts project textures into validated BC7 sRGB QTexture files.
     * @ingroup quiet_cooker
     * @return False when any processed texture reports a required-operation failure.
     * @note Skips built-in and non-Texture records; continues after asset failures.
     * @note Cache reuse requires matching metadata, output hash, and reader validation.
     * @note Conversion generates sRGB mips and uses BC7 quality 0.05.
     * @note Output hashing failure prevents caching without failing conversion.
     * @note Does not roll back filesystem writes on failure.
     */
    bool CookProjectTextures(CookContext& context);
}
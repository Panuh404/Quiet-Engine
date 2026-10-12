#pragma once

#include "Core/CookContext.h"

namespace Quiet::Cooker
{
    /**
     * @brief Packages native WAV, FLAC, and MP3 assets without transcoding.
     * @ingroup quiet_cooker
     * @return False when any processed asset reports a required-operation failure.
     * @note Skips built-in and non-Audio records; continues after asset failures.
     * @note Requires matching source/output hashes and decoder metadata inspection.
     * @note Successful assets append runtime records and next-cache entries.
     * @note Does not validate every decoded frame or roll back output writes.
     */
    bool CookProjectAudio(CookContext& context);
}
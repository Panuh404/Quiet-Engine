#pragma once

#include "Core/CookContext.h"

namespace Quiet::Cooker
{
    /**
     * @brief Packages registered Lua source files without compilation.
     * @ingroup quiet_cooker
     * @return False when any processed asset reports a required-operation failure.
     * @note Skips built-in and non-Script records; continues after asset failures.
     * @note Requires matching source and packaged-output hashes.
     * @note Does not parse Lua or validate runtime callbacks.
     * @note Successful assets append runtime records and next-cache entries.
     * @note Does not roll back output writes on failure.
     */
    bool CookProjectScripts(CookContext& context);
}
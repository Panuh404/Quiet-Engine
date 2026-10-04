#pragma once

#include <string>

#include "Core/CookContext.h"

namespace Quiet::Cooker
{
    bool ValidateCompressonatorBC7(std::string& outError);

    bool CookProjectTextures(CookContext& context);
}
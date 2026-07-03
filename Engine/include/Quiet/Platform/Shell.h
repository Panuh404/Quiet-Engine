#pragma once

#include <filesystem>

namespace Quiet::Platform
{
    bool RevealPathInFileManager(const std::filesystem::path& path);
}
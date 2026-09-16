#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace Quiet::Platform
{
    bool OpenPathWithDefaultApplication(const std::filesystem::path& path);
    bool RevealPathInFileManager(const std::filesystem::path& path);

    [[nodiscard]] std::filesystem::path GetExecutablePath();

    bool LaunchDetachedProcess(const std::filesystem::path& executable, const std::vector<std::string>& arguments = {});

}
#include "Quiet/Platform/Shell.h"

#include "Quiet/Core/Log.h"

#include <cstdlib>
#include <filesystem>
#include <string>

#if defined(_WIN32)
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif

    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif

    #include <windows.h>
    #include <shellapi.h>
#endif

namespace
{
#if !defined(_WIN32)
    std::string ShellQuoteUnix(const std::string& value)
    {
        std::string quoted = "'";
        for (const char c : value)
        {
            if (c == '\'') quoted += "'\\''";
            else quoted += c;
        }

        quoted += "'";
        return quoted;
    }
#endif
}

namespace Quiet::Platform
{
    bool RevealPathInFileManager(const std::filesystem::path& path)
    {
        if (path.empty()) return false;
        const std::filesystem::path normalizedPath = std::filesystem::absolute(path).lexically_normal();

        if (!std::filesystem::exists(normalizedPath))
        {
            QUIET_CORE_WARN("Cannot reveal path because it does not exist: '{}'.", normalizedPath.string());
            return false;
        }

#if defined(_WIN32)
        const std::wstring widePath = normalizedPath.wstring();
        HINSTANCE result = nullptr;

        if (std::filesystem::is_directory(normalizedPath))
        {
            result = ShellExecuteW(
                nullptr,
                L"open",
                widePath.c_str(),
                nullptr,
                nullptr,
                SW_SHOWNORMAL
            );
        }
        else
        {
            const std::wstring parameters =
                L"/select,\"" + widePath + L"\"";

            result = ShellExecuteW(
                nullptr,
                L"open",
                L"explorer.exe",
                parameters.c_str(),
                nullptr,
                SW_SHOWNORMAL
            );
        }

        const auto resultCode = reinterpret_cast<std::intptr_t>(result);
        if (resultCode <= 32)
        {
            QUIET_CORE_WARN(
                "Failed to reveal path in Explorer: '{}'. ShellExecuteW result: {}.",
                normalizedPath.string(),
                resultCode
            );

            return false;
        }

        return true;

#elif defined(__APPLE__)
        const std::string command =
            std::filesystem::is_directory(normalizedPath)
                ? "open " + ShellQuoteUnix(normalizedPath.string())
                : "open -R " + ShellQuoteUnix(normalizedPath.string());

        return std::system(command.c_str()) == 0;

#else
        const std::filesystem::path directory =
            std::filesystem::is_directory(normalizedPath)
                ? normalizedPath
                : normalizedPath.parent_path();

        if (directory.empty())
        {
            return false;
        }

        const std::string command =
            "xdg-open " + ShellQuoteUnix(directory.string());

        return std::system(command.c_str()) == 0;
#endif
    }
}
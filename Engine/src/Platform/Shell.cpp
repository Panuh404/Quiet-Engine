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

#if defined(__APPLE__)
    #include <mach-o/dyld.h>
    #include <sys/types.h>
    #include <sys/wait.h>
    #include <unistd.h>

#elif !defined(_WIN32)
    #include <sys/types.h>
    #include <sys/wait.h>
    #include <unistd.h>
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
            const std::wstring parameters = L"/select,\"" + widePath + L"\"";

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
            QUIET_CORE_WARN("Failed to reveal path in Explorer: '{}'. ShellExecuteW result: {}.", normalizedPath.string(), resultCode);
            return false;
        }
        return true;

    #elif defined(__APPLE__)
        const std::string command = std::filesystem::is_directory(normalizedPath)
            ? "open " + ShellQuoteUnix(normalizedPath.string())
            : "open -R " + ShellQuoteUnix(normalizedPath.string());

        return std::system(command.c_str()) == 0;

    #else
        const std::filesystem::path directory = std::filesystem::is_directory(normalizedPath)
            ? normalizedPath
            : normalizedPath.parent_path();

        if (directory.empty())
        {
            return false;
        }

        const std::string command = "xdg-open " + ShellQuoteUnix(directory.string());
        return std::system(command.c_str()) == 0;
    #endif
    }

    std::filesystem::path GetExecutablePath()
    {
    #if defined(_WIN32)

        std::wstring buffer(32768, L'\0');
        const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));

        if (length == 0 || length >= buffer.size())
        {
            return {};
        }

        buffer.resize(length);
        return std::filesystem::path(buffer);

    #elif defined(__APPLE__)
        std::uint32_t requiredSize = 0;
        _NSGetExecutablePath(nullptr, &requiredSize);

        if (requiredSize == 0)
            return {};

        std::string buffer(requiredSize, '\0');
        if (_NSGetExecutablePath(buffer.data(), &requiredSize) != 0)
        {
            return {};
        }

        return std::filesystem::weakly_canonical(std::filesystem::path(buffer.c_str()));

    #else

        std::string buffer(4096, '\0');
        const ssize_t length = readlink("/proc/self/exe", buffer.data(), buffer.size() - 1);

        if (length <= 0)
            return {};

        buffer.resize(static_cast<std::size_t>(length));

        return std::filesystem::path(buffer);

    #endif
    }

    bool LaunchDetachedProcess(const std::filesystem::path& executable, const std::vector<std::string>& arguments)
    {
        if (executable.empty())
            return false;

        const std::filesystem::path normalizedExecutable = std::filesystem::absolute(executable).lexically_normal();
        if (!std::filesystem::exists(normalizedExecutable))
        {
            QUIET_CORE_WARN("Cannot launch process because the executable does not exist: '{}'.", normalizedExecutable.string());
            return false;
        }

    #if defined(_WIN32)

        std::wstring parameters;

        for (const std::string& argument : arguments)
        {
            if (!parameters.empty()) parameters += L' ';
            parameters += L'"';
            parameters += std::filesystem::path(argument).wstring();
            parameters += L'"';
        }

        const std::wstring executablePath = normalizedExecutable.wstring();
        const std::wstring workingDirectory = normalizedExecutable.parent_path().wstring();

        HINSTANCE result = ShellExecuteW(
            nullptr,
            L"open",
            executablePath.c_str(),
            parameters.empty() ? nullptr : parameters.c_str(),
            workingDirectory.empty() ? nullptr : workingDirectory.c_str(),
            SW_SHOWNORMAL);

        const auto resultCode = reinterpret_cast<std::intptr_t>(result);
        if (resultCode <= 32)
        {
            QUIET_CORE_WARN("Failed to launch process '{}'. ShellExecuteW result: {}.", normalizedExecutable.string(), resultCode);
            return false;
        }

        return true;

    #else
        const pid_t firstChild = fork();
        if (firstChild < 0) return false;
        if (firstChild == 0)
        {
            const pid_t secondChild = fork();
            if (secondChild < 0) _exit(127);
            if (secondChild > 0) _exit(0);
            if (setsid() < 0) _exit(127);

            const std::filesystem::path workingDirectory = normalizedExecutable.parent_path();
            if (!workingDirectory.empty())
            {
                (void)chdir(workingDirectory.string().c_str());
            }

            std::vector<std::string> argumentStorage;
            argumentStorage.reserve(arguments.size() + 1);
            argumentStorage.push_back(normalizedExecutable.string());

            for (const std::string& argument : arguments)
            {
                argumentStorage.push_back(argument);
            }

            std::vector<char*> argumentPointers;
            argumentPointers.reserve(argumentStorage.size() + 1);
            for (std::string& argument : argumentStorage)
            {
                argumentPointers.push_back(argument.data());
            }
            argumentPointers.push_back(nullptr);

            execv(normalizedExecutable.string().c_str(), argumentPointers.data());
            _exit(127);
        }
        int status = 0;

        if (waitpid(firstChild, &status, 0) < 0)
        {
            return false;
        }

        return WIFEXITED(status) && WEXITSTATUS(status) == 0;
    #endif
    }
}
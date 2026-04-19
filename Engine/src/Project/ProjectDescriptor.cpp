#include "Quiet/Project/ProjectDescriptor.h"

#include <ctime>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <utility>

#include <nlohmann/json.hpp>

namespace Quiet::Project
{
    namespace
    {
        using json = nlohmann::json;
    }

    std::string ProjectDescriptorIO::GetCurrentUtcTimestampString()
    {
        const std::time_t now = std::time(nullptr);

        std::tm utcTime{};
    #if defined(_WIN32)
        gmtime_s(&utcTime, &now);
    #else
        gmtime_r(&now, &utcTime);
    #endif

        std::ostringstream stream;
        stream << std::put_time(&utcTime, "%Y-%m-%dT%H:%M:%SZ");
        return stream.str();
    }

    ProjectDescriptor ProjectDescriptorIO::MakeDefault(std::string projectName)
    {
        ProjectDescriptor descriptor{};
        descriptor.Name = projectName.empty() ? "QuietProject" : std::move(projectName);

        const std::string nowUtc = GetCurrentUtcTimestampString();
        descriptor.CreatedAtUtc = nowUtc;
        descriptor.UpdatedAtUtc = nowUtc;

        return descriptor;
    }

    bool ProjectDescriptorIO::LoadFromFile(const std::filesystem::path& path, ProjectDescriptor& outDescriptor)
    {
        std::ifstream input(path);
        if (!input.is_open())
        {
            return false;
        }

        json root;
        try
        {
            input >> root;
        }
        catch (...)
        {
            return false;
        }

        if (!root.is_object())
        {
            return false;
        }

        ProjectDescriptor descriptor{};

        descriptor.Name = root.value("Name", descriptor.Name);
        descriptor.Version = root.value("Version", descriptor.Version);
        descriptor.CreatedAtUtc = root.value("CreatedAtUtc", descriptor.CreatedAtUtc);
        descriptor.UpdatedAtUtc = root.value("UpdatedAtUtc", descriptor.UpdatedAtUtc);
        descriptor.StartupScene = root.value("StartupScene", descriptor.StartupScene);
        descriptor.ScenesDirectory = root.value("ScenesDirectory", descriptor.ScenesDirectory);
        descriptor.AssetsDirectory = root.value("AssetsDirectory", descriptor.AssetsDirectory);
        descriptor.CacheDirectory = root.value("CacheDirectory", descriptor.CacheDirectory);

        if (descriptor.Name.empty()) descriptor.Name = "QuietProject";
        if (descriptor.StartupScene.empty()) descriptor.StartupScene = "DefaultScene.json";
        if (descriptor.ScenesDirectory.empty()) descriptor.ScenesDirectory = "Scenes";
        if (descriptor.AssetsDirectory.empty()) descriptor.AssetsDirectory = "Assets";
        if (descriptor.CacheDirectory.empty()) descriptor.CacheDirectory = "Cache";

        if (descriptor.CreatedAtUtc.empty())
        {
            descriptor.CreatedAtUtc = GetCurrentUtcTimestampString();
        }

        if (descriptor.UpdatedAtUtc.empty())
        {
            descriptor.UpdatedAtUtc = descriptor.CreatedAtUtc;
        }

        outDescriptor = std::move(descriptor);
        return true;
    }

    bool ProjectDescriptorIO::SaveToFile(const std::filesystem::path& path, ProjectDescriptor descriptor)
    {
        std::error_code ec;
        std::filesystem::create_directories(path.parent_path(), ec);

        if (descriptor.CreatedAtUtc.empty())
        {
            descriptor.CreatedAtUtc = GetCurrentUtcTimestampString();
        }

        descriptor.UpdatedAtUtc = GetCurrentUtcTimestampString();

        std::ofstream output(path);
        if (!output.is_open())
        {
            return false;
        }

        json root;
        root["Name"] = descriptor.Name;
        root["Version"] = descriptor.Version;
        root["CreatedAtUtc"] = descriptor.CreatedAtUtc;
        root["UpdatedAtUtc"] = descriptor.UpdatedAtUtc;
        root["StartupScene"] = descriptor.StartupScene;
        root["ScenesDirectory"] = descriptor.ScenesDirectory;
        root["AssetsDirectory"] = descriptor.AssetsDirectory;
        root["CacheDirectory"] = descriptor.CacheDirectory;

        output << root.dump(4);
        return output.good();
    }
}
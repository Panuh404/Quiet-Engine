#include "Core/CookCache.h"

#include <array>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <utility>

#include <nlohmann/json.hpp>

namespace Quiet::Cooker
{
    namespace
    {
        using json = nlohmann::json;
        constexpr std::uint64_t FnvOffsetBasis = 14695981039346656037ull;
        constexpr std::uint64_t FnvPrime = 1099511628211ull;

        std::string HashToString(const std::uint64_t hash)
        {
            std::ostringstream stream;
            stream << std::hex << std::setfill('0') << std::setw(16) << hash;
            return stream.str();
        }
    }

    bool ComputeFileHash(const std::filesystem::path& path, std::string& outHash, std::string& outError)
    {
        outHash.clear();
        outError.clear();

        std::ifstream input(path, std::ios::binary);
        if (!input.is_open())
        {
            outError = "Failed to open source file for hashing: " + path.string();
            return false;
        }

        std::uint64_t hash = FnvOffsetBasis;
        std::array<char, 64 * 1024> buffer{};
        while (input)
        {
            input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
            const std::streamsize bytesRead = input.gcount();

            for (std::streamsize index = 0; index < bytesRead; ++index)
            {
                hash ^= static_cast<std::uint8_t>(buffer[static_cast<std::size_t>(index)]);
                hash *= FnvPrime;
            }
        }

        if (!input.eof())
        {
            outError = "Failed while hashing source file: " + path.string();
            return false;
        }

        outHash = HashToString(hash);
        return true;
    }

    bool LoadCookCache(const std::filesystem::path& path, CookCache& outCache, std::string& outError)
    {
        outCache = {};
        outError.clear();

        if (!std::filesystem::exists(path)) return true;
        std::ifstream input(path);
        if (!input.is_open())
        {
            outError = "Failed to open cook cache: " + path.string();
            return false;
        }

        try
        {
            json root;
            input >> root;

            const std::uint32_t version = root.at("Version").get<std::uint32_t>();
            if (version != CurrentCookCacheVersion)
            {
                // An incompatible cache is equivalent to no cache.
                outCache = {};
                return true;
            }

            CookCache cache{};
            cache.Version = version;
            const json& assets = root.at("Assets");

            if (!assets.is_object())
            {
                outError = "Cook cache Assets must be an object.";
                return false;
            }

            for (auto iterator = assets.begin(); iterator != assets.end(); ++iterator)
            {
                const std::string assetID = iterator.key();
                const json& value = iterator.value();

                Project::AssetType type = Project::AssetType::Unknown;
                if (!Project::AssetTypeFromString(value.at("Type").get<std::string>(), type))
                {
                    outError = "Cook cache contains an unknown asset type.";
                    return false;
                }

                CookCacheEntry entry{};
                entry.Type = type;
                entry.SourceHash = value.at("SourceHash").get<std::string>();
                entry.CookedHash = value.at("CookedHash").get<std::string>();
                entry.CookSignature = value.at("CookSignature").get<std::string>();
                entry.RuntimePath = value.at("RuntimePath").get<std::string>();

                if (assetID.empty() || !entry.IsValid())
                {
                    outError = "Cook cache contains an invalid asset entry.";
                    return false;
                }

                cache.Assets.emplace(assetID, std::move(entry));
            }

            outCache = std::move(cache);
            return true;
        }
        catch (const json::exception& exception)
        {
            outError = std::string("Failed to parse cook cache: ") + exception.what();
            return false;
        }
    }

    bool SaveCookCache(const std::filesystem::path& path, const CookCache& cache, std::string& outError)
    {
        outError.clear();

        std::error_code ec;

        if (!path.parent_path().empty())
        {
            std::filesystem::create_directories(path.parent_path(), ec);
            if (ec)
            {
                outError = "Failed to create cook cache directory: " + ec.message();
                return false;
            }
        }

        json root;
        root["Version"] = CurrentCookCacheVersion;
        root["Assets"] = json::object();

        for (const auto& [assetID, entry] : cache.Assets)
        {
            if (assetID.empty() || !entry.IsValid()) continue;
            root["Assets"][assetID] =
            {
                { "Type", std::string(Project::ToString(entry.Type)) },
                { "SourceHash", entry.SourceHash },
                { "CookedHash", entry.CookedHash },
                { "CookSignature", entry.CookSignature },
                { "RuntimePath", entry.RuntimePath }
            };
        }

        std::ofstream output(path, std::ios::trunc);
        if (!output.is_open())
        {
            outError = "Failed to open cook cache for writing: " + path.string();
            return false;
        }

        output << root.dump(4);
        if (!output.good())
        {
            outError = "Failed to write cook cache: " + path.string();
            return false;
        }

        return true;
    }
}
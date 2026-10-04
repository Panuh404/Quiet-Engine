#include "Script/ScriptCooker.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>
#include <utility>

namespace Quiet::Cooker
{
    namespace
    {
        constexpr std::string_view ScriptCookSignature = "script-lua-text-copy-v1";

        std::filesystem::path ResolveAssetPath(const std::filesystem::path& projectRoot, const Project::AssetRecord& record)
        {
            if (record.Path.empty()) return {};
            const std::filesystem::path rawPath = record.Path;

            return rawPath.is_absolute() ? rawPath.lexically_normal() : (projectRoot / rawPath).lexically_normal();
        }

        std::string NormalizeScriptExtension(const std::filesystem::path& path)
        {
            std::string extension = path.extension().string();
            std::transform(
                extension.begin(),
                extension.end(),
                extension.begin(),
                [](const unsigned char value)
                {
                    return static_cast<char>(std::tolower(value));
                });

            if (extension == ".lua") return extension;
            return {};
        }
    }

    bool CookProjectScripts(CookContext& context)
    {
        const auto& projectRoot = context.ProjectRoot;
        const auto& outputRoot = context.OutputRoot;
        const auto& registry = context.AssetRegistry;
        const auto& previousCookCache = context.PreviousCache;
        auto& outNextCookCache = context.NextCache;
        auto& outRuntimeAssets = context.RuntimeAssets;

        bool success = true;

        for (const Project::AssetRecord& record : registry.Records)
        {
            if (record.BuiltIn || record.Type != Project::AssetType::Script) continue;

            const std::filesystem::path sourcePath = ResolveAssetPath(projectRoot, record);
            if (sourcePath.empty() || !std::filesystem::is_regular_file(sourcePath))
            {
                std::cerr << "Script '" << record.ID << "' points to a missing source file: " << sourcePath << '\n';
                success = false;
                continue;
            }

            const std::string extension = NormalizeScriptExtension(sourcePath);
            if (extension.empty())
            {
                std::cerr << "Script '" << record.ID << "' uses an unsupported format: " << sourcePath.extension().string() << '\n';
                success = false;
                continue;
            }

            const std::filesystem::path runtimePath = std::filesystem::path("Content") / "Scripts" / (record.ID + extension);
            const std::filesystem::path cookedPath = outputRoot / runtimePath;

            std::string sourceHash;
            std::string sourceHashError;

            if (!ComputeFileHash(sourcePath, sourceHash, sourceHashError))
            {
                std::cerr << "Failed to fingerprint script '" << record.ID << "': " << sourceHashError << '\n';
                success = false;
                continue;
            }

            const auto cachedIterator = previousCookCache.Assets.find(record.ID);
            if (cachedIterator != previousCookCache.Assets.end())
            {
                const CookCacheEntry& cached = cachedIterator->second;
                const std::string runtimePathString = runtimePath.generic_string();

                const bool metadataMatches =
                    cached.Type == Project::AssetType::Script &&
                    cached.SourceHash == sourceHash &&
                    cached.CookSignature == ScriptCookSignature &&
                    cached.RuntimePath == runtimePathString;

                if (metadataMatches)
                {
                    std::error_code cacheFileError;
                    const bool cookedFileExists = std::filesystem::is_regular_file(cookedPath, cacheFileError) && !cacheFileError;
                    if (cookedFileExists)
                    {
                        std::string currentCookedHash;
                        std::string cookedHashError;

                        const bool cookedHashSucceeded = ComputeFileHash(cookedPath, currentCookedHash, cookedHashError);

                        if (cookedHashSucceeded &&
                            currentCookedHash == cached.CookedHash &&
                            currentCookedHash == sourceHash)
                        {
                            Project::RuntimeAssetRecord runtimeRecord{};
                            runtimeRecord.ID = record.ID;
                            runtimeRecord.Type = Project::AssetType::Script;
                            runtimeRecord.Path = runtimePathString;

                            outRuntimeAssets.push_back(std::move(runtimeRecord));
                            outNextCookCache.Assets[record.ID] = cached;

                            std::uint64_t fileSize = 0;
                            cacheFileError.clear();

                            const std::uintmax_t cachedFileSize = std::filesystem::file_size(cookedPath, cacheFileError);
                            if (!cacheFileError)
                            {
                                fileSize = static_cast<std::uint64_t>(cachedFileSize);
                            }

                            std::cout
                                << "Script: " << record.ID << '\n'
                                << "  Source:      " << sourcePath.filename().string() << '\n'
                                << "  Cook:        up to date\n"
                                << "  Language:    Lua\n"
                                << "  File size:   " << fileSize << " bytes\n"
                                << "  Runtime:     " << cookedPath.filename().string() << '\n'
                                << "  Cache:       hit\n\n";

                            continue;
                        }

                        if (!cookedHashSucceeded)
                        {
                            std::cerr << "Could not fingerprint cached script '" << record.ID << "': " << cookedHashError << ". Repackaging.\n";
                        }
                    }
                }
            }

            std::error_code copyError;

            std::filesystem::copy_file(
                sourcePath,
                cookedPath,
                std::filesystem::copy_options::overwrite_existing,
                copyError);

            if (copyError)
            {
                std::cerr << "Failed to package script '" << record.ID << "': " << copyError.message() << '\n';
                success = false;
                continue;
            }

            std::string cookedHash;
            std::string cookedHashError;

            if (!ComputeFileHash(cookedPath, cookedHash, cookedHashError))
            {
                std::cerr << "Failed to fingerprint packaged script '" << record.ID << "': " << cookedHashError << '\n';
                success = false;
                continue;
            }

            if (cookedHash != sourceHash)
            {
                std::cerr << "Packaged script '" << record.ID << "' changed during copy.\n";
                success = false;
                continue;
            }

            Project::RuntimeAssetRecord runtimeRecord{};
            runtimeRecord.ID = record.ID;
            runtimeRecord.Type = Project::AssetType::Script;
            runtimeRecord.Path = runtimePath.generic_string();

            outRuntimeAssets.push_back(std::move(runtimeRecord));

            CookCacheEntry cacheEntry{};
            cacheEntry.Type = Project::AssetType::Script;
            cacheEntry.SourceHash = sourceHash;
            cacheEntry.CookedHash = cookedHash;
            cacheEntry.CookSignature = std::string(ScriptCookSignature);
            cacheEntry.RuntimePath = runtimePath.generic_string();

            outNextCookCache.Assets[record.ID] = std::move(cacheEntry);

            std::error_code sizeError;
            const std::uintmax_t fileSize = std::filesystem::file_size(cookedPath, sizeError);

            std::cout
                << "Script: " << record.ID << '\n'
                << "  Source:      " << sourcePath.filename().string() << '\n'
                << "  Language:    Lua\n"
                << "  File size:   " << (sizeError ? 0 : fileSize) << " bytes\n"
                << "  Runtime:     " << cookedPath.filename().string() << '\n'
                << "  Copy:        byte-for-byte\n"
                << "  Validation:  hash match\n"
                << "  Cache:       miss (packaged)\n\n";
        }

        return success;
    }
}
#include "Audio/AudioCooker.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>
#include <utility>

#include <miniaudio.h>

namespace Quiet::Cooker
{
    namespace
    {
        constexpr std::string_view AudioCookSignature = "audio-native-copy-v1";

        struct AudioCookStats
        {
            std::string Encoding;

            std::uint32_t Channels = 0;
            std::uint32_t SampleRate = 0;

            std::uint64_t FileSizeBytes = 0;
        };

        std::filesystem::path ResolveAssetPath(const std::filesystem::path& projectRoot, const Project::AssetRecord& record)
        {
            if (record.Path.empty()) return {};
            const std::filesystem::path rawPath = record.Path;

            return rawPath.is_absolute() ? rawPath.lexically_normal() : (projectRoot / rawPath).lexically_normal();
        }

        std::string NormalizeAudioExtension(const std::filesystem::path& path)
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

            if (extension == ".wav" || extension == ".flac" || extension == ".mp3") return extension;
            return {};
        }

        std::string AudioEncodingFromExtension(const std::string_view extension)
        {
            if (extension == ".wav")    return "WAV";
            if (extension == ".flac")   return "FLAC";
            if (extension == ".mp3")    return "MP3";
            return "Unknown";
        }

        bool InspectAudioFile(const std::filesystem::path& path, AudioCookStats& outStats, std::string& outError)
        {
            outStats = {};
            outError.clear();

            const std::string extension = NormalizeAudioExtension(path);
            if (extension.empty())
            {
                outError = "Unsupported audio extension: " + path.extension().string();
                return false;
            }

            const std::string nativePath = path.string();
            ma_decoder decoder{};

            const ma_result initializeResult = ma_decoder_init_file(nativePath.c_str(), nullptr, &decoder);
            if (initializeResult != MA_SUCCESS)
            {
                outError = std::string("miniaudio failed to open the audio file: ") + ma_result_description(initializeResult);
                return false;
            }

            ma_format sampleFormat = ma_format_unknown;
            ma_uint32 channels = 0;
            ma_uint32 sampleRate = 0;

            const ma_result formatResult = ma_decoder_get_data_format(&decoder, &sampleFormat, &channels, &sampleRate, nullptr, 0);
            ma_decoder_uninit(&decoder);

            if (formatResult != MA_SUCCESS)
            {
                outError = std::string("miniaudio failed to read audio metadata: ") + ma_result_description(formatResult);
                return false;
            }

            if (channels == 0 || sampleRate == 0)
            {
                outError = "Audio file contains invalid channel or sample-rate metadata.";
                return false;
            }

            std::error_code ec;
            const std::uintmax_t fileSize = std::filesystem::file_size(path, ec);
            if (ec)
            {
                outError = "Failed to determine audio file size: " + ec.message();
                return false;
            }

            outStats.Encoding = AudioEncodingFromExtension(extension);
            outStats.Channels = static_cast<std::uint32_t>(channels);
            outStats.SampleRate = static_cast<std::uint32_t>(sampleRate);
            outStats.FileSizeBytes = static_cast<std::uint64_t>(fileSize);
            return true;
        }
    }

    bool CookProjectAudio(CookContext& context)
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
            if (record.BuiltIn || record.Type != Project::AssetType::Audio) continue;

            const std::filesystem::path sourcePath = ResolveAssetPath(projectRoot, record);
            if (sourcePath.empty() || !std::filesystem::is_regular_file(sourcePath))
            {
                std::cerr << "Audio '" << record.ID << "' points to a missing source file: " << sourcePath << '\n';
                success = false;
                continue;
            }

            const std::string extension = NormalizeAudioExtension(sourcePath);
            if (extension.empty())
            {
                std::cerr << "Audio '" << record.ID << "' uses an unsupported format: " << sourcePath.extension().string() << '\n';
                success = false;
                continue;
            }

            const std::filesystem::path runtimePath = std::filesystem::path("Content") / "Audio" / (record.ID + extension);
            const std::filesystem::path cookedPath = outputRoot / runtimePath;

            std::string sourceHash;
            std::string sourceHashError;

            if (!ComputeFileHash(sourcePath, sourceHash, sourceHashError))
            {
                std::cerr << "Failed to fingerprint audio '" << record.ID << "': " << sourceHashError << '\n';
                success = false;
                continue;
            }

            const auto cachedIterator = previousCookCache.Assets.find(record.ID);
            if (cachedIterator != previousCookCache.Assets.end())
            {
                const CookCacheEntry& cached = cachedIterator->second;
                const std::string runtimePathString = runtimePath.generic_string();

                const bool metadataMatches =
                    cached.Type == Project::AssetType::Audio &&
                    cached.SourceHash == sourceHash &&
                    cached.CookSignature == AudioCookSignature &&
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
                            AudioCookStats stats{};
                            std::string inspectError;

                            if (InspectAudioFile(cookedPath, stats, inspectError))
                            {
                                Project::RuntimeAssetRecord runtimeRecord{};
                                runtimeRecord.ID = record.ID;
                                runtimeRecord.Type = Project::AssetType::Audio;
                                runtimeRecord.Path = runtimePathString;

                                outRuntimeAssets.push_back(std::move(runtimeRecord));
                                outNextCookCache.Assets[record.ID] = cached;

                                std::cout
                                    << "Audio: " << record.ID << '\n'
                                    << "  Source:      " << sourcePath.filename().string() << '\n'
                                    << "  Cook:        up to date\n"
                                    << "  Format:      " << stats.Encoding << '\n'
                                    << "  Channels:    " << stats.Channels << '\n'
                                    << "  Sample rate: " << stats.SampleRate << " Hz\n"
                                    << "  File size:   " << stats.FileSizeBytes << " bytes\n"
                                    << "  Runtime:     " << cookedPath.filename().string() << '\n'
                                    << "  Cache:       hit\n\n";

                                continue;
                            }

                            std::cerr << "Cached audio asset '" << record.ID << "' failed validation: " << inspectError << ". Recooking.\n";
                        }
                        else if (!cookedHashSucceeded)
                        {
                            std::cerr << "Could not fingerprint cached audio '" << record.ID << "': " << cookedHashError << ". Recooking.\n";
                        }
                    }
                }
            }

            AudioCookStats sourceStats{};
            std::string inspectError;

            if (!InspectAudioFile(sourcePath, sourceStats, inspectError))
            {
                std::cerr << "Failed to validate audio '" << record.ID << "': " << inspectError << '\n';
                success = false;
                continue;
            }

            std::error_code copyError;
            std::filesystem::copy_file(sourcePath, cookedPath, std::filesystem::copy_options::overwrite_existing, copyError);
            if (copyError)
            {
                std::cerr << "Failed to package audio '" << record.ID << "': " << copyError.message() << '\n';
                success = false;
                continue;
            }

            std::string cookedHash;
            std::string cookedHashError;

            if (!ComputeFileHash(cookedPath, cookedHash, cookedHashError))
            {
                std::cerr << "Failed to fingerprint packaged audio '" << record.ID << "': " << cookedHashError << '\n';
                success = false;
                continue;
            }

            if (cookedHash != sourceHash)
            {
                std::cerr << "Packaged audio '" << record.ID << "' changed during copy.\n";
                success = false;
                continue;
            }

            AudioCookStats cookedStats{};
            std::string cookedInspectError;

            if (!InspectAudioFile(cookedPath, cookedStats, cookedInspectError))
            {
                std::cerr << "Packaged audio '" << record.ID << "' failed validation: " << cookedInspectError << '\n';
                success = false;
                continue;
            }

            Project::RuntimeAssetRecord runtimeRecord{};
            runtimeRecord.ID = record.ID;
            runtimeRecord.Type = Project::AssetType::Audio;
            runtimeRecord.Path = runtimePath.generic_string();

            outRuntimeAssets.push_back(std::move(runtimeRecord));

            CookCacheEntry cacheEntry{};
            cacheEntry.Type = Project::AssetType::Audio;
            cacheEntry.SourceHash = sourceHash;
            cacheEntry.CookedHash = cookedHash;
            cacheEntry.CookSignature = std::string(AudioCookSignature);
            cacheEntry.RuntimePath = runtimePath.generic_string();

            outNextCookCache.Assets[record.ID] = std::move(cacheEntry);

            std::cout
                << "Audio: " << record.ID << '\n'
                << "  Source:      " << sourcePath.filename().string() << '\n'
                << "  Format:      " << cookedStats.Encoding << '\n'
                << "  Channels:    " << cookedStats.Channels << '\n'
                << "  Sample rate: " << cookedStats.SampleRate << " Hz\n"
                << "  File size:   " << cookedStats.FileSizeBytes << " bytes\n"
                << "  Runtime:     " << cookedPath.filename().string() << '\n'
                << "  Copy:        byte-for-byte\n"
                << "  Validation:  OK\n"
                << "  Cache:       miss (packaged)\n\n";
        }

        return success;
    }
}
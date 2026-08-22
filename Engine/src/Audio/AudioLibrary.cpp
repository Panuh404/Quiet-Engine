#include "Audio/AudioLibrary.h"

#include <algorithm>
#include <cctype>
#include <system_error>

#include <miniaudio.h>

#include "Quiet/Core/Log.h"

namespace
{
    std::string DetectEncoding(const std::filesystem::path& path)
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

        if (extension == ".wav")    return "WAV";
        if (extension == ".flac")   return "FLAC";
        if (extension == ".mp3")    return "MP3";
        return "Unknown";
    }
}

namespace Quiet::Audio
{
    bool AudioLibrary::LoadFromFile(const std::string& assetID, const std::filesystem::path& filePath, const bool replaceExisting)
    {
        if (assetID.empty() || filePath.empty()) return false;
        if (Has(assetID) && !replaceExisting) return true;

        std::error_code error;
        if (!std::filesystem::exists(filePath, error) || error)
        {
            QUIET_CORE_WARN("Audio asset '{}' points to missing file '{}'.", assetID, filePath.string());
            return false;
        }

        if (!std::filesystem::is_regular_file(filePath, error) || error)
        {
            QUIET_CORE_WARN("Audio asset '{}' does not point to a regular file.", assetID);
            return false;
        }

        const std::string path = filePath.string();
        ma_decoder decoder{};

        const ma_result initializeResult = ma_decoder_init_file(path.c_str(), nullptr, &decoder);
        if (initializeResult != MA_SUCCESS)
        {
            QUIET_CORE_WARN("Failed to inspect audio asset '{}' from '{}': {}", assetID, path, ma_result_description(initializeResult));
            return false;
        }

        ma_format sampleFormat = ma_format_unknown;
        ma_uint32 channels = 0;
        ma_uint32 sampleRate = 0;

        const ma_result formatResult = ma_decoder_get_data_format(&decoder, &sampleFormat, &channels, &sampleRate, nullptr, 0);
        if (formatResult != MA_SUCCESS || channels == 0 || sampleRate == 0)
        {
            ma_decoder_uninit(&decoder);
            QUIET_CORE_WARN("Failed to read audio metadata for '{}'.", assetID);
            return false;
        }

        AudioAsset asset{};
        asset.SourcePath = filePath.lexically_normal();
        asset.Encoding = DetectEncoding(filePath);
        asset.Channels = channels;
        asset.SampleRate = sampleRate;

        error.clear();
        asset.FileSizeBytes = std::filesystem::file_size(filePath, error);
        if (error) asset.FileSizeBytes = 0;

        /*
         * Do not calculate MP3 duration during project bootstrap.
         *
         * miniaudio may need to scan/decode the complete MP3 to
         * determine its total frame count. Long music assets would
         * therefore make project loading unnecessarily expensive.
         */
        if (asset.Encoding != "MP3")
        {
            ma_uint64 frameCount = 0;
            if (ma_decoder_get_length_in_pcm_frames(&decoder, &frameCount) == MA_SUCCESS)
            {
                asset.FrameCount = frameCount;
                asset.DurationSeconds = static_cast<double>(frameCount) / static_cast<double>(sampleRate);
                asset.DurationValid = true;
            }
        }

        ma_decoder_uninit(&decoder);
        if (!asset.IsValid()) return false;
        m_Assets[assetID] = asset;

        if (asset.DurationValid)
        {
            QUIET_CORE_INFO("Loaded audio asset '{}'. format={} channels={} sampleRate={}Hz duration={:.2f}s bytes={}",
                assetID, asset.Encoding, asset.Channels, asset.SampleRate, asset.DurationSeconds, asset.FileSizeBytes);
        }
        else
        {
            QUIET_CORE_INFO("Loaded audio asset '{}'. format={} channels={} sampleRate={}Hz duration=unknown bytes={}",
                assetID, asset.Encoding, asset.Channels, asset.SampleRate, asset.FileSizeBytes);
        }

        return true;
    }

    std::optional<AudioAsset> AudioLibrary::Get(const std::string_view assetID) const
    {
        const auto iterator = m_Assets.find(std::string(assetID));
        if (iterator == m_Assets.end()) return std::nullopt;
        return iterator->second;
    }

    bool AudioLibrary::Has(const std::string_view assetID) const
    {
        return m_Assets.contains(std::string(assetID));
    }

    std::vector<std::string> AudioLibrary::GetNames() const
    {
        std::vector<std::string> names;
        names.reserve(m_Assets.size());

        for (const auto& [name, asset] : m_Assets)
        {
            (void)asset;
            names.push_back(name);
        }

        std::sort(names.begin(), names.end());
        return names;
    }

    void AudioLibrary::Clear()
    {
        m_Assets.clear();
    }
}
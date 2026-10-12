#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

#include "Quiet/Assets/MeshImportResult.h"

namespace Quiet::Cooker
{
    /**
     * @brief Reports mesh packing, encoding, payload, and file byte counts.
     * @ingroup quiet_cooker
     */
    struct QMeshWriteStats
    {
        std::uint64_t RawVertexBytes = 0;
        std::uint64_t PackedVertexBytes = 0;
        std::uint64_t EncodedVertexBytes = 0;

        std::uint64_t RawIndexBytes = 0;
        std::uint64_t EncodedIndexBytes = 0;

        std::uint64_t UncompressedPayloadBytes = 0;
        std::uint64_t StoredPayloadBytes = 0;

        std::uint64_t FileBytes = 0;

        bool ZstdCompressed = false;
    };

    /**
     * @brief Writes quantized, meshoptimizer-encoded geometry in a QAsset container.
     * @ingroup quiet_cooker
     * @return False on reported input, encoding, compression, or write failure.
     * @note Requires valid bounds and nonempty geometry within format count limits.
     * @note Uses Zstandard only when its output is smaller.
     * @note Caller must prepare the parent directory.
     * @note Truncates the destination without atomic replacement.
     * @note Resets statistics but does not clear outError on entry.
     * @note Does not perform read-back validation.
     */
    bool WriteQMesh(const std::filesystem::path& path, const Assets::MeshImportResult& mesh, QMeshWriteStats& outStats, std::string& outError);
}
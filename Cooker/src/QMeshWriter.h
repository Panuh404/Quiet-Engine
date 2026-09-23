#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

#include "Quiet/Assets/MeshImportResult.h"

namespace Quiet::Cooker
{
    struct QMeshWriteStats
    {
        std::uint64_t RawVertexBytes = 0;
        std::uint64_t EncodedVertexBytes = 0;

        std::uint64_t RawIndexBytes = 0;
        std::uint64_t EncodedIndexBytes = 0;

        std::uint64_t UncompressedPayloadBytes = 0;
        std::uint64_t StoredPayloadBytes = 0;

        std::uint64_t FileBytes = 0;

        bool ZstdCompressed = false;
    };

    bool WriteQMesh(const std::filesystem::path& path, const Assets::MeshImportResult& mesh, QMeshWriteStats& outStats, std::string& outError);
}
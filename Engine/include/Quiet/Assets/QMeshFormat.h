#pragma once

#include <cstdint>
#include <type_traits>

namespace Quiet::Assets
{
    inline constexpr std::uint32_t CurrentQMeshPayloadVersion = 1;

    enum class QMeshVertexFormat : std::uint32_t
    {
        Float32PositionColorNormalTexCoord = 1,
        QuantizedPosition16OctNormal16Color8HalfTexCoord = 2
    };

    struct QMeshVertexV1
    {
        float Position[3];
        float Color[3];
        float Normal[3];
        float TexCoord[2];
    };

    struct QMeshSubmeshV1
    {
        std::uint32_t BaseVertex = 0;
        std::uint32_t VertexCount = 0;

        std::uint32_t BaseIndex = 0;
        std::uint32_t IndexCount = 0;

        std::uint32_t MaterialSlot = 0;
    };

    struct QMeshVertexQuantizedV1
    {
        // Bounds-relative UNORM16.
        std::uint16_t Position[3];
        std::uint16_t PositionPadding = 0;

        // Octahedral SNORM16.
        std::int16_t NormalOct[2];

        // UNORM8 RGB + reserved alpha.
        std::uint8_t Color[4];

        // IEEE-754 half precision.
        std::uint16_t TexCoordHalf[2];
    };

    static_assert(std::is_trivially_copyable_v<QMeshVertexV1>);
    static_assert(std::is_trivially_copyable_v<QMeshSubmeshV1>);
    static_assert(sizeof(QMeshVertexV1) == 44, "QMeshVertexV1 disk layout must remain 44 bytes.");
    static_assert(sizeof(QMeshSubmeshV1) == 20, "QMeshSubmeshV1 disk layout must remain 20 bytes.");
    
    static_assert(std::is_trivially_copyable_v<QMeshVertexQuantizedV1>);
    static_assert(sizeof(QMeshVertexQuantizedV1) == 20, "Quantized QMesh vertex layout must remain 20 bytes."
    );
}
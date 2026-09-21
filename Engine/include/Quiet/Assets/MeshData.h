#pragma once

#include <cstdint>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

namespace Quiet::Assets
{
    struct MeshVertex
    {
        glm::vec3 Position;
        glm::vec3 Color;
        glm::vec3 Normal;
        glm::vec2 TexCoord;
    };

    using MeshIndex = std::uint32_t;

    struct MeshBounds
    {
        glm::vec3 Min{ 0.0f };
        glm::vec3 Max{ 0.0f };
        bool Valid = false;
    };
}
#pragma once

#include <optional>
#include <string_view>

#include "Quiet/Core/UUID.h"

namespace Quiet::Project::BuiltInAssetIDs
{
    // Reserved engine identities. Once used by project data, these values must remain stable across engine versions and projects.
    inline constexpr Core::UUID Triangle    { 0x51554945544D4553ULL, 0x0000000000000001ULL };
    inline constexpr Core::UUID Quad        { 0x51554945544D4553ULL, 0x0000000000000002ULL };
    inline constexpr Core::UUID Plane       { 0x51554945544D4553ULL, 0x0000000000000003ULL };
    inline constexpr Core::UUID Cube        { 0x51554945544D4553ULL, 0x0000000000000004ULL };
    inline constexpr Core::UUID Pyramid     { 0x51554945544D4553ULL, 0x0000000000000005ULL };
    inline constexpr Core::UUID Sphere      { 0x51554945544D4553ULL, 0x0000000000000006ULL };
    inline constexpr Core::UUID Cylinder    { 0x51554945544D4553ULL, 0x0000000000000007ULL };

    // Used only when translating the existing name-based asset format.
    [[nodiscard]] constexpr std::optional<Core::UUID> FromLegacyMeshID(const std::string_view legacyID)
    {
        if (legacyID == "Triangle") return Triangle;
        if (legacyID == "Quad")     return Quad;
        if (legacyID == "Plane")    return Plane;
        if (legacyID == "Cube")     return Cube;
        if (legacyID == "Pyramid")  return Pyramid;
        if (legacyID == "Sphere")   return Sphere;
        if (legacyID == "Cylinder") return Cylinder;
        return std::nullopt;
    }
}
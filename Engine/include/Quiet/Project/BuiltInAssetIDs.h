#pragma once

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

    // Renderer-owned texture. This ID is reserved outside the project registry.
    inline constexpr Core::UUID EditorTopbarLogo{ 0x5155494554544558ULL, 0x0000000000000001ULL };
}
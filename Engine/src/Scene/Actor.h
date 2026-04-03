#pragma once

#include <optional>
#include <string>

#include "RotatorComponent.h"
#include "StaticMeshComponent.h"
#include "TransformComponent.h"

namespace Quiet::Scene
{
    struct Actor
    {
        std::string Name;
        TransformComponent Transform {};
        std::optional<StaticMeshComponent> StaticMesh;
        std::optional<RotatorComponent> Rotator;
    };
}
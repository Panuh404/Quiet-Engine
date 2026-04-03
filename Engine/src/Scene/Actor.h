#pragma once

#include <optional>
#include <string>

#include "Scene/RotatorComponent.h"
#include "Scene/StaticMeshComponent.h"
#include "Scene/TransformComponent.h"

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
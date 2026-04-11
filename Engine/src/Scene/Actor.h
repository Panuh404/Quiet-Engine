#pragma once

#include <optional>
#include <string>

#include "Quiet/Core/UUID.h"
#include "Scene/RotatorComponent.h"
#include "Scene/StaticMeshComponent.h"
#include "Scene/TransformComponent.h"

namespace Quiet::Scene
{
    struct Actor
    {
        Core::UUID ID{};
        std::string Name;
        TransformComponent Transform {};
        std::optional<StaticMeshComponent> StaticMesh;
        std::optional<RotatorComponent> Rotator;
    };
}
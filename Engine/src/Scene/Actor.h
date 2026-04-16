#pragma once

#include <optional>
#include <string>

#include "CameraComponent.h"
#include "Quiet/Core/UUID.h"
#include "Scene/Components/RotatorComponent.h"
#include "Scene/Components/StaticMeshComponent.h"
#include "Scene/Components/TransformComponent.h"

namespace Quiet::Scene
{
    struct Actor
    {
        Core::UUID ID{};
        std::string Name;
        TransformComponent Transform {};
        std::optional<StaticMeshComponent> StaticMesh;
        std::optional<RotatorComponent> Rotator;
        std::optional<CameraComponent> Camera;
    };
}
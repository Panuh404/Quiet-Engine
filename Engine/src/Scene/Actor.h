#pragma once

#include <optional>
#include <string>

#include "Quiet/Core/UUID.h"
#include "Scene/Components/CameraComponent.h"
#include "Scene/Components/RotatorComponent.h"
#include "Scene/Components/StaticMeshComponent.h"
#include "Scene/Components/TransformComponent.h"
#include "Scene/Components/MaterialComponent.h"

namespace Quiet::Scene
{
    struct Actor
    {
        Core::UUID ID{};
        std::string Name;
        TransformComponent Transform {};
        std::optional<StaticMeshComponent> StaticMesh;
        std::optional<MaterialComponent> Material;
        std::optional<RotatorComponent> Rotator;
        std::optional<CameraComponent> Camera;
    };
}
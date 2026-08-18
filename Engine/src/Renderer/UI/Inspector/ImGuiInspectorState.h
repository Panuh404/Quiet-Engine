#pragma once

#include <string>
#include <vector>

#include <imgui.h>

#include "Scene/Components/BoxColliderComponent.h"
#include "Scene/Components/CameraComponent.h"
#include "Scene/Components/CapsuleColliderComponent.h"
#include "Scene/Components/DirectionalLightComponent.h"
#include "Scene/Components/MaterialComponent.h"
#include "Scene/Components/RigidBodyComponent.h"
#include "Scene/Components/RotatorComponent.h"
#include "Scene/Components/SphereColliderComponent.h"
#include "Scene/Components/TransformComponent.h"

namespace Quiet::Renderer::UI::Inspector
{
    enum class ComponentClipboardType
    {
        None,
        Transform,
        StaticMesh,
        Material,
        DirectionalLight,
        Rotator,
        Camera,

        RigidBody,
        BoxCollider,
        SphereCollider,
        CapsuleCollider
    };

    struct ComponentClipboardState
    {
        ComponentClipboardType Type = ComponentClipboardType::None;
        Scene::TransformComponent Transform{};

        std::string StaticMeshResourceID;
        bool StaticMeshVisible = true;

        Scene::MaterialComponent Material{};
        std::vector<Scene::MaterialComponent> MaterialSlots{};
        Scene::DirectionalLightComponent DirectionalLight{};
        Scene::RotatorComponent Rotator{};
        Scene::CameraComponent Camera{};
        Scene::RigidBodyComponent RigidBody{};
        Scene::BoxColliderComponent BoxCollider{};
        Scene::SphereColliderComponent SphereCollider{};
        Scene::CapsuleColliderComponent CapsuleCollider{};
    };

    struct InspectorState
    {
        ComponentClipboardState Clipboard{};
        std::string OpenComponentMenuID{};
        ImVec2 OpenComponentMenuPosition{ 0.0f, 0.0f };
        int OpenComponentMenuFrame = -1;
    };
}
#pragma once

#include <cassert>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>

#include "Quiet/Core/UUID.h"
#include "Scene/Components/CameraComponent.h"
#include "Scene/Components/DirectionalLightComponent.h"
#include "Scene/Components/MaterialComponent.h"
#include "Scene/Components/RotatorComponent.h"
#include "Scene/Components/StaticMeshComponent.h"
#include "Scene/Components/TransformComponent.h"

namespace Quiet::Scene
{
    template<typename>
    inline constexpr bool UnsupportedActorComponent = false;

    struct Actor
    {
        Core::UUID ID{};
        std::string Name;

        TransformComponent Transform {};
        std::optional<StaticMeshComponent> StaticMesh;
        std::optional<MaterialComponent> Material;
        std::optional<DirectionalLightComponent> DirectionalLight;
        std::optional<RotatorComponent> Rotator;
        std::optional<CameraComponent> Camera;

        [[nodiscard]] Core::UUID GetID() const { return ID; }
        [[nodiscard]] std::string& GetName() { return Name; }
        [[nodiscard]] const std::string& GetName() const { return Name; }

        template<typename T>
        [[nodiscard]] bool HasComponent() const
        {
            if constexpr (std::is_same_v<T, TransformComponent>)                return true;
            else if constexpr (std::is_same_v<T, StaticMeshComponent>)          return StaticMesh.has_value();
            else if constexpr (std::is_same_v<T, MaterialComponent>)            return Material.has_value();
            else if constexpr (std::is_same_v<T, DirectionalLightComponent>)    return DirectionalLight.has_value();
            else if constexpr (std::is_same_v<T, RotatorComponent>)             return Rotator.has_value();
            else if constexpr (std::is_same_v<T, CameraComponent>)              return Camera.has_value();
            else static_assert(UnsupportedActorComponent<T>, "Unsupported Quiet Actor component type.");
        }

        template<typename T>
        [[nodiscard]] T& GetComponent()
        {
            if constexpr (std::is_same_v<T, TransformComponent>)                return Transform;
            else if constexpr (std::is_same_v<T, StaticMeshComponent>)          return StaticMesh.value();
            else if constexpr (std::is_same_v<T, MaterialComponent>)            return Material.value();
            else if constexpr (std::is_same_v<T, DirectionalLightComponent>)    return DirectionalLight.value();
            else if constexpr (std::is_same_v<T, RotatorComponent>)             return Rotator.value();
            else if constexpr (std::is_same_v<T, CameraComponent>)              return Camera.value();
            else static_assert(UnsupportedActorComponent<T>, "Unsupported Quiet Actor component type.");
        }

        template<typename T>
        [[nodiscard]] const T& GetComponent() const
        {
            if constexpr (std::is_same_v<T, TransformComponent>)                return Transform;
            else if constexpr (std::is_same_v<T, StaticMeshComponent>)          return StaticMesh.value();
            else if constexpr (std::is_same_v<T, MaterialComponent>)            return Material.value();
            else if constexpr (std::is_same_v<T, DirectionalLightComponent>)    return DirectionalLight.value();
            else if constexpr (std::is_same_v<T, RotatorComponent>)             return Rotator.value();
            else if constexpr (std::is_same_v<T, CameraComponent>)              return Camera.value();
            else static_assert(UnsupportedActorComponent<T>, "Unsupported Quiet Actor component type.");
        }

        template<typename T>
        [[nodiscard]] T* TryGetComponent()
        {
            if (!HasComponent<T>()) return nullptr;
            return &GetComponent<T>();
        }

        template<typename T>
        [[nodiscard]] const T* TryGetComponent() const
        {
            if (!HasComponent<T>()) return nullptr;
            return &GetComponent<T>();
        }

        template<typename T, typename... Args>
        T& AddComponent(Args&&... args)
        {
            static_assert(!std::is_same_v<T, TransformComponent>, "TransformComponent is mandatory and cannot be added.");
            assert(!HasComponent<T>());

            if constexpr (std::is_same_v<T, StaticMeshComponent>)               return StaticMesh.emplace(std::forward<Args>(args)...);
            else if constexpr (std::is_same_v<T, MaterialComponent>)            return Material.emplace(std::forward<Args>(args)...);
            else if constexpr (std::is_same_v<T, DirectionalLightComponent>)    return DirectionalLight.emplace(std::forward<Args>(args)...);
            else if constexpr (std::is_same_v<T, RotatorComponent>)             return Rotator.emplace(std::forward<Args>(args)...);
            else if constexpr (std::is_same_v<T, CameraComponent>)              return Camera.emplace(std::forward<Args>(args)...);
            else static_assert(UnsupportedActorComponent<T>, "Unsupported Quiet Actor component type.");
        }

        template<typename T>
        bool RemoveComponent()
        {
            static_assert(!std::is_same_v<T, TransformComponent>, "TransformComponent is mandatory and cannot be removed.");

            if constexpr (std::is_same_v<T, StaticMeshComponent>)
            {
                const bool existed = StaticMesh.has_value();
                StaticMesh.reset();
                return existed;
            }
            else if constexpr (std::is_same_v<T, MaterialComponent>)
            {
                const bool existed = Material.has_value();
                Material.reset();
                return existed;
            }
            else if constexpr (std::is_same_v<T, DirectionalLightComponent>)
            {
                const bool existed = DirectionalLight.has_value();
                DirectionalLight.reset();
                return existed;
            }
            else if constexpr (std::is_same_v<T, RotatorComponent>)
            {
                const bool existed = Rotator.has_value();
                Rotator.reset();
                return existed;
            }
            else if constexpr (std::is_same_v<T, CameraComponent>)
            {
                const bool existed = Camera.has_value();
                Camera.reset();
                return existed;
            }
            else
            {
                static_assert(UnsupportedActorComponent<T>, "Unsupported Quiet Actor component type.");
            }
        }
    };
}
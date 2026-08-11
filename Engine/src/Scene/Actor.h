#pragma once

#include <cassert>
#include <string>
#include <type_traits>
#include <utility>

#include <entt/entity/registry.hpp>

#include "Quiet/Core/UUID.h"
#include "Scene/Components/CameraComponent.h"
#include "Scene/Components/DirectionalLightComponent.h"
#include "Scene/Components/MaterialComponent.h"
#include "Scene/Components/RotatorComponent.h"
#include "Scene/Components/StaticMeshComponent.h"
#include "Scene/Components/TransformComponent.h"

namespace Quiet::Scene
{
    class Scene;

    class Actor
    {
        friend class Scene;

    public:
        Actor(const Actor&) = delete;
        Actor& operator=(const Actor&) = delete;

        Actor(Actor&&) noexcept = default;
        Actor& operator=(Actor&&) noexcept = default;

        [[nodiscard]] Core::UUID GetID() const { return m_ID; }
        [[nodiscard]] std::string& GetName() { return m_Name; }
        [[nodiscard]] const std::string& GetName() const { return m_Name; }

        template<typename T>
        [[nodiscard]] bool HasComponent() const
        {
            assert(m_Registry && m_Registry->valid(m_Entity));
            return m_Registry ->all_of<T>(m_Entity);
        }

        template<typename T>
        [[nodiscard]] T& GetComponent()
        {
            assert(m_Registry && m_Registry->valid(m_Entity));
            return m_Registry->get<T>(m_Entity);
        }

        template<typename T>
        [[nodiscard]] const T& GetComponent() const
        {
            assert(m_Registry && m_Registry->valid(m_Entity));
            return static_cast<const entt::registry&>(*m_Registry).get<T>(m_Entity);
        }

        template<typename T>
        [[nodiscard]] T* TryGetComponent()
        {
            assert(m_Registry && m_Registry->valid(m_Entity));
            return m_Registry->try_get<T>(m_Entity);
        }

        template<typename T>
        [[nodiscard]] const T* TryGetComponent() const
        {
            assert(m_Registry && m_Registry->valid(m_Entity));
            return static_cast<const entt::registry&>(*m_Registry).try_get<T>(m_Entity);
        }

        template<typename T, typename... Args>
        T& AddComponent(Args&&... args)
        {
            static_assert(!std::is_same_v<T, TransformComponent>, "TransformComponent is mandatory and cannot be added.");
            assert(m_Registry && m_Registry->valid(m_Entity));
            assert(!HasComponent<T>());
            return m_Registry->emplace<T>(m_Entity, std::forward<Args>(args)...);
        }

        template<typename T>
        bool RemoveComponent()
        {
            static_assert(!std::is_same_v<T, TransformComponent>, "TransformComponent is mandatory and cannot be removed.");
            assert(m_Registry && m_Registry->valid(m_Entity));
            return m_Registry->remove<T>(m_Entity) != 0;
        }

    private:
        Actor(entt::registry& registry, entt::entity entity, Core::UUID id, std::string name)
            : m_Registry(&registry),
              m_Entity(entity),
              m_ID(id),
              m_Name(std::move(name)) {}

    private:
        entt::registry* m_Registry = nullptr;
        entt::entity m_Entity = entt::null;

        Core::UUID m_ID{};
        std::string m_Name;
    };
}
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
#include "Scene/Components/ActorIdentityComponent.h"
#include "Scene/Components/BoxColliderComponent.h"
#include "Scene/Components/RigidBodyComponent.h"

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

        [[nodiscard]] Core::UUID GetID() const { return GetIdentityComponent().ID; }
        [[nodiscard]] std::string& GetName() { return GetIdentityComponent().Name; }
        [[nodiscard]] const std::string& GetName() const { return GetIdentityComponent().Name; }

        template<typename T>
        [[nodiscard]] bool HasComponent() const
        {
            static_assert(!std::is_same_v<std::remove_cv_t<T>, ActorIdentityComponent>, "ActorIdentityComponent is internal to Scene.");
            assert(m_Registry && m_Registry->valid(m_Entity));
            return m_Registry ->all_of<T>(m_Entity);
        }

        template<typename T>
        [[nodiscard]] T& GetComponent()
        {
            static_assert(!std::is_same_v<std::remove_cv_t<T>, ActorIdentityComponent>, "ActorIdentityComponent is internal to Scene.");
            assert(m_Registry && m_Registry->valid(m_Entity));
            return m_Registry->get<T>(m_Entity);
        }

        template<typename T>
        [[nodiscard]] const T& GetComponent() const
        {
            static_assert(!std::is_same_v<std::remove_cv_t<T>, ActorIdentityComponent>, "ActorIdentityComponent is internal to Scene.");
            assert(m_Registry && m_Registry->valid(m_Entity));
            return static_cast<const entt::registry&>(*m_Registry).get<T>(m_Entity);
        }

        template<typename T>
        [[nodiscard]] T* TryGetComponent()
        {
            static_assert(!std::is_same_v<std::remove_cv_t<T>, ActorIdentityComponent>, "ActorIdentityComponent is internal to Scene.");
            assert(m_Registry && m_Registry->valid(m_Entity));
            return m_Registry->try_get<T>(m_Entity);
        }

        template<typename T>
        [[nodiscard]] const T* TryGetComponent() const
        {
            static_assert(!std::is_same_v<std::remove_cv_t<T>, ActorIdentityComponent>, "ActorIdentityComponent is internal to Scene.");
            assert(m_Registry && m_Registry->valid(m_Entity));
            return static_cast<const entt::registry&>(*m_Registry).try_get<T>(m_Entity);
        }

        template<typename T, typename... Args>
        T& AddComponent(Args&&... args)
        {
            static_assert(
                !std::is_same_v<std::remove_cv_t<T>, TransformComponent> &&
                !std::is_same_v<std::remove_cv_t<T>, ActorIdentityComponent>,
                "Mandatory Actor components cannot be added or removed.");
            assert(m_Registry && m_Registry->valid(m_Entity));
            assert(!HasComponent<T>());
            return m_Registry->emplace<T>(m_Entity, std::forward<Args>(args)...);
        }

        template<typename T>
        bool RemoveComponent()
        {
            static_assert(
                !std::is_same_v<std::remove_cv_t<T>, TransformComponent> &&
                !std::is_same_v<std::remove_cv_t<T>, ActorIdentityComponent>,
                "Mandatory Actor components cannot be added or removed.");
            assert(m_Registry && m_Registry->valid(m_Entity));
            return m_Registry->remove<T>(m_Entity) != 0;
        }

    private:
        [[nodiscard]] ActorIdentityComponent& GetIdentityComponent()
        {
            assert(m_Registry && m_Registry->valid(m_Entity));
            return m_Registry->get<ActorIdentityComponent>(m_Entity);
        }

        [[nodiscard]]
        const ActorIdentityComponent&
        GetIdentityComponent() const
        {
            assert(m_Registry && m_Registry->valid(m_Entity));
            return static_cast<const entt::registry&>(*m_Registry) .get<ActorIdentityComponent>(m_Entity);
        }

        Actor(entt::registry& registry, entt::entity entity)
            : m_Registry(&registry), m_Entity(entity) {}

    private:
        entt::registry* m_Registry = nullptr;
        entt::entity m_Entity = entt::null;
    };
}
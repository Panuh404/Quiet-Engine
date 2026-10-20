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
#include "Scene/Components/SphereColliderComponent.h"
#include "Scene/Components/CapsuleColliderComponent.h"
#include "Scene/Components/RigidBodyComponent.h"
#include "Scene/Components/AudioSourceComponent.h"
#include "Scene/Components/AudioListenerComponent.h"
#include "Scene/Components/ScriptComponent.h"
#include "Scene/Components/TextComponent.h"

namespace Quiet::Scene
{
    class Scene;

    /**
     * @brief Provides component access to an entity owned by a Scene.
     * @ingroup quiet_scene
     *
     * Component and identity data remain owned by the Scene's registry.
     * The handle is non-copyable and movable.
     *
     * @note Identity and component access require a live owning Scene
     *       and a valid entity.
     * @note Pointers and references to Scene-held Actors follow the
     *       invalidation rules of the Scene's Actor vector.
     */

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

        /**
         * @brief Tests whether the Actor has the requested component.
         * @tparam T Component type other than ActorIdentityComponent.
         * @pre The owning registry is alive and the entity is valid.
         * @return Whether the component is present.
         */
        template<typename T>
        [[nodiscard]] bool HasComponent() const
        {
            static_assert(!std::is_same_v<std::remove_cv_t<T>, ActorIdentityComponent>, "ActorIdentityComponent is internal to Scene.");
            assert(m_Registry && m_Registry->valid(m_Entity));
            return m_Registry ->all_of<T>(m_Entity);
        }

        /**
         * @brief Returns the requested component from the owning registry.
         * @tparam T Component type other than ActorIdentityComponent.
         * @pre The owning registry is alive and the entity is valid.
         * @pre The Actor has component T.
         * @return A reference to the registry-owned component.
         */
        template<typename T>
        [[nodiscard]] T& GetComponent()
        {
            static_assert(!std::is_same_v<std::remove_cv_t<T>, ActorIdentityComponent>, "ActorIdentityComponent is internal to Scene.");
            assert(m_Registry && m_Registry->valid(m_Entity));
            return m_Registry->get<T>(m_Entity);
        }

        /**
         * @brief Returns the requested component from the owning registry.
         * @tparam T Component type other than ActorIdentityComponent.
         * @pre The owning registry is alive and the entity is valid.
         * @pre The Actor has component T.
         * @return A const reference to the registry-owned component.
         */
        template<typename T>
        [[nodiscard]] const T& GetComponent() const
        {
            static_assert(!std::is_same_v<std::remove_cv_t<T>, ActorIdentityComponent>, "ActorIdentityComponent is internal to Scene.");
            assert(m_Registry && m_Registry->valid(m_Entity));
            return static_cast<const entt::registry&>(*m_Registry).get<T>(m_Entity);
        }

        /**
         * @brief Looks up an optional component on a valid Actor.
         * @tparam T Component type other than ActorIdentityComponent.
         * @pre The owning registry is alive and the entity is valid.
         * @return A registry-owned component pointer, or nullptr if absent.
         * @note Missing-component handling does not validate an expired Actor.
         */
        template<typename T>
        [[nodiscard]] T* TryGetComponent()
        {
            static_assert(!std::is_same_v<std::remove_cv_t<T>, ActorIdentityComponent>, "ActorIdentityComponent is internal to Scene.");
            assert(m_Registry && m_Registry->valid(m_Entity));
            return m_Registry->try_get<T>(m_Entity);
        }

        /**
         * @brief Looks up a component on a valid Actor without permitting mutation.
         * @tparam T Component type other than ActorIdentityComponent.
         * @pre The owning registry is alive and the entity is valid.
         * @return A const registry-owned component pointer, or nullptr if absent.
         * @note Missing-component handling does not validate an expired Actor.
         */
        template<typename T>
        [[nodiscard]] const T* TryGetComponent() const
        {
            static_assert(!std::is_same_v<std::remove_cv_t<T>, ActorIdentityComponent>, "ActorIdentityComponent is internal to Scene.");
            assert(m_Registry && m_Registry->valid(m_Entity));
            return static_cast<const entt::registry&>(*m_Registry).try_get<T>(m_Entity);
        }

        /**
         * @brief Constructs an optional component in the owning registry.
         * @tparam T Component type other than TransformComponent or
         *           ActorIdentityComponent.
         * @param args Arguments forwarded to component construction.
         * @pre The owning registry is alive and the entity is valid.
         * @pre The Actor does not already have component T.
         * @return A reference to the newly constructed component.
         */
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

        /**
         * @brief Removes an optional component from the owning registry.
         * @tparam T Component type other than TransformComponent or
         *           ActorIdentityComponent.
         * @pre The owning registry is alive and the entity is valid.
         * @return True if a component was removed; false if it was absent.
         */
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
#include "qtpch.h"
#include "Quiet/Scene/Entity.h"

namespace Quiet
{
	Entity::Entity(entt::entity handle, Scene* scene)
		: m_EntityHandle(handle), m_Scene(scene)
	{}
}
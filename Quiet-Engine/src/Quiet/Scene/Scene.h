#pragma once

#include "entt.hpp"
#include "Quiet/Core/Timestep.h"

namespace Quiet
{
	class Scene
	{
	public:
		Scene();
		~Scene();

		// TEMP
		entt::entity CreateEntity();
		entt::registry& Reg() { return m_Registry; }

		void OnUpdate(Timestep ts);
	private:
		entt::registry m_Registry;
	};
}

#pragma once
#include "ComponentsCommon.h"

struct entity_info
{
	
};

namespace Quiet
{
	u32 create_game_entity(const entity_info& info);
	void remove_game_entity(u32 id);
}
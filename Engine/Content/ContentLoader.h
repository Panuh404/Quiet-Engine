#pragma once

#include "CommonHeaders.h"

#if !defined(SHIPPING)
namespace quiet::content
{
	bool load_game();
	void unload_game();

	
}
#endif

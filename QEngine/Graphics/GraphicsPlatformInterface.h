#pragma once
#include "CommonHeaders.h"
#include "Renderer.h"

namespace quiet::graphics
{
	struct platform_interface
	{
		bool(*initialize)(void);
		void(*shutdown)(void);
	};
}
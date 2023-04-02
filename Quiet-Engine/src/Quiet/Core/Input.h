#pragma once

#include "Quiet/Core/Core.h"
#include "Quiet/Core/KeyCodes.h"
#include "Quiet/Core/MouseCodes.h"

namespace Quiet
{
	class Input
	{
	public:
		static bool IsKeyPressed(KeyCode key);
		static bool IsMouseButtonPressed(MouseCode button);
		static std::pair<float, float> GetMousePosition();
		static float GetMouseX();
		static float GetMouseY();
	};
}
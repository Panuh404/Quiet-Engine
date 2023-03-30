#pragma once

namespace Quiet
{
	//-----------------------------------------------------------------------------
	// [CLASS] Renderer Context
	//-----------------------------------------------------------------------------
	class GraphicsContext
	{
	public:
		virtual void Init() = 0;
		virtual void SwapBuffers() = 0;

		static std::unique_ptr<GraphicsContext> Create(void* window);
	};
}

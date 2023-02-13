#pragma once

namespace Quiet
{
	//-----------------------------------------------------------------------------
	// [CLASS] Renderer Context
	//-----------------------------------------------------------------------------
	class RendererContext
	{
	public:
		virtual void Init() = 0;
		virtual void SwapBuffers() = 0;
		static std::unique_ptr<RendererContext> Create(void* window);
	};
}

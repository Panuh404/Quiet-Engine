#pragma once

namespace Quiet
{
	enum class RendererAPI
	{
		None		= 0,
		OpenGL		= 1,
		Vulkan		= 2,	// Not Implemented Yet
		DirectX11	= 3,	// Not Implemented Yet
		DirectX12	= 4		// Not Implemented Yet
	};

	class Renderer
	{
	public:
		inline static RendererAPI GetAPI() { return s_RendererAPI; }

	private:
		static RendererAPI s_RendererAPI;
	};
}
#pragma once

#include "Quiet/Renderer/VertexArray.h"

#include <glm/glm.hpp>


namespace Quiet
{
	//-----------------------------------------------------------------------------
	// [CLASS] Renderer API
	//-----------------------------------------------------------------------------
	class RendererAPI
	{
	public:
		enum class API
		{
			None		= 0,
			OpenGL		= 1,
			Vulkan		= 2,
			DirectX11	= 3,
			DirectX12	= 4
		};

		virtual ~RendererAPI() = default;

		virtual void Init() = 0;
		virtual void SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height) = 0;
		virtual void SetClearColor(const glm::vec4& color) = 0;
		virtual void Clear() = 0;

		virtual void DrawIndexed(const std::shared_ptr<VertexArray>& vertexArray, uint32_t indexCount = 0) = 0;

		static API GetAPI() { return s_API; }
		static std::unique_ptr<RendererAPI> Create();

	private:
		static API s_API;
	};
}
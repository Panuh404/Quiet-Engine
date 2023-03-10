#include "qtpch.h"

#include "Quiet/Renderer/VertexArray.h"
#include "Quiet/Renderer/Renderer.h"

#include "Platform/OpenGL/OpenGLVertexArray.h"

namespace Quiet
{
	//-----------------------------------------------------------------------------
	// [API Abstraction] Vertex Array
	//-----------------------------------------------------------------------------
	std::shared_ptr<VertexArray> VertexArray::Create()
	{
		switch (Renderer::GetAPI())
		{
			case RendererAPI::API::None:		QT_CORE_ASSERT(false, "RendererAPI::There is no API Selected"); return nullptr;
			case RendererAPI::API::OpenGL:		return std::make_shared<OpenGLVertexArray>();
			case RendererAPI::API::Vulkan:		QT_CORE_ASSERT(false, "RendererAPI::Vulkan is currently not supported!"); return nullptr;
			case RendererAPI::API::DirectX11:	QT_CORE_ASSERT(false, "RendererAPI::Direct11 is currently not supported!"); return nullptr;
			case RendererAPI::API::DirectX12:	QT_CORE_ASSERT(false, "RendererAPI::Direct12 is currently not supported!"); return nullptr;
		}
		QT_CORE_ASSERT(false, "RendererAPI::Unknown RendererAPI");
		return nullptr;
	}

}
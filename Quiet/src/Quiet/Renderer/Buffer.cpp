#include "qtpch.h"

#include "Quiet/Renderer/Buffer.h"
#include "Quiet/Renderer/Renderer.h"

#include "Platform/OpenGL/OpenGLBuffer.h"

namespace Quiet
{
	//-----------------------------------------------------------------------------
	// [API Abstraction] Vertex Buffer
	//-----------------------------------------------------------------------------
	VertexBuffer* VertexBuffer::Create(float* vertices, uint32_t size)
	{
		switch (Renderer::GetAPI())
		{
		case RendererAPI::None:			QT_CORE_ASSERT(false, "RendererAPI::There is no API Selected"); return nullptr;
		case RendererAPI::OpenGL:		return new OpenGLVertexBuffer(vertices, size);
		case RendererAPI::Vulkan:		QT_CORE_ASSERT(false, "RendererAPI::Vulkan is currently not supported!"); return nullptr;
		case RendererAPI::DirectX11:	QT_CORE_ASSERT(false, "RendererAPI::Direct11 is currently not supported!"); return nullptr;
		case RendererAPI::DirectX12:	QT_CORE_ASSERT(false, "RendererAPI::Direct12 is currently not supported!"); return nullptr;
		}
		QT_CORE_ASSERT(false, "RendererAPI::Unknown RendererAPI");
		return nullptr;
	}

	//-----------------------------------------------------------------------------
	// [API Abstraction] Index Buffer
	//-----------------------------------------------------------------------------
	IndexBuffer* IndexBuffer::Create(uint32_t* indices, uint32_t size)
	{
		switch (Renderer::GetAPI())
		{
		case RendererAPI::None:			QT_CORE_ASSERT(false, "RendererAPI::There is no API Selected"); return nullptr;
		case RendererAPI::OpenGL:		return new OpenGLIndexBuffer(indices, size);
		case RendererAPI::Vulkan:		QT_CORE_ASSERT(false, "RendererAPI::Vulkan is currently not supported!"); return nullptr;
		case RendererAPI::DirectX11:	QT_CORE_ASSERT(false, "RendererAPI::Direct11 is currently not supported!"); return nullptr;
		case RendererAPI::DirectX12:	QT_CORE_ASSERT(false, "RendererAPI::Direct12 is currently not supported!"); return nullptr;
		}

		QT_CORE_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
	}
}
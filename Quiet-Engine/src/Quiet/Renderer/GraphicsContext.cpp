#include "qtpch.h"

#include "Quiet/Renderer/GraphicsContext.h"

#include "Quiet/Renderer/Renderer.h"
#include "Platform/OpenGL/OpenGLContext.h"

namespace Quiet
{
	//-----------------------------------------------------------------------------
	// [API Abstraction] Graphic Context
	//-----------------------------------------------------------------------------
	std::unique_ptr<GraphicsContext> GraphicsContext::Create(void* window)
	{
		switch (Renderer::GetAPI())
		{
			case RendererAPI::API::None:		QT_CORE_ASSERT(false, "RendererAPI::There is no API Selected"); return nullptr;
			case RendererAPI::API::OpenGL:		return std::make_unique<OpenGLContext>(static_cast<GLFWwindow*>(window));
			case RendererAPI::API::Vulkan:		QT_CORE_ASSERT(false, "RendererAPI::Vulkan is currently not supported!"); return nullptr;
			case RendererAPI::API::DirectX11:	QT_CORE_ASSERT(false, "RendererAPI::Direct11 is currently not supported!"); return nullptr;
			case RendererAPI::API::DirectX12:	QT_CORE_ASSERT(false, "RendererAPI::Direct12 is currently not supported!"); return nullptr;
		}
		QT_CORE_ASSERT(false, "RendererAPI::Unknown RendererAPI");
		return nullptr;
	}
}
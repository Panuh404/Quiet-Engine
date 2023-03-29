#include "qtpch.h"

#include "Quiet/Renderer/Framebuffer.h"
#include "Quiet/Renderer/Renderer.h"

#include "Platform/OpenGL/OpenGLFramebuffer.h"

namespace Quiet
{
	//-----------------------------------------------------------------------------
	// [API Abstraction] Framebuffer
	//-----------------------------------------------------------------------------
	std::shared_ptr<Framebuffer> Framebuffer::Create(const FramebufferSpecification& spec)
	{
		switch (Renderer::GetAPI())
		{
		case RendererAPI::API::None:		QT_CORE_ASSERT(false, "RendererAPI::There is no API Selected"); return nullptr;
		case RendererAPI::API::OpenGL:		return std::make_shared<OpenGLFramebuffer>(spec);
		case RendererAPI::API::Vulkan:		QT_CORE_ASSERT(false, "RendererAPI::Vulkan is currently not supported!"); return nullptr;
		case RendererAPI::API::DirectX11:	QT_CORE_ASSERT(false, "RendererAPI::Direct11 is currently not supported!"); return nullptr;
		case RendererAPI::API::DirectX12:	QT_CORE_ASSERT(false, "RendererAPI::Direct12 is currently not supported!"); return nullptr;
		}
		QT_CORE_ASSERT(false, "RendererAPI::Unknown RendererAPI");
		return nullptr;
	}
}
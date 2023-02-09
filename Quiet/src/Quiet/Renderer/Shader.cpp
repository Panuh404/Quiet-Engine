#include "qtpch.h"

#include "Quiet/Renderer/Shader.h"
#include "Quiet/Renderer/Renderer.h"

#include "Platform/OpenGL/OpenGLShader.h"

namespace Quiet
{
	//-----------------------------------------------------------------------------
	// [API Abstraction] Create Shader with File
	//-----------------------------------------------------------------------------
	Shader* Shader::Create(const std::string& filepath)
	{
		switch (Renderer::GetAPI())
		{
		case RendererAPI::API::None:		QT_CORE_ASSERT(false, "RendererAPI::There is no API Selected"); return nullptr;
		case RendererAPI::API::OpenGL:		return new OpenGLShader(filepath);
		case RendererAPI::API::Vulkan:		QT_CORE_ASSERT(false, "RendererAPI::Vulkan is currently not supported!"); return nullptr;
		case RendererAPI::API::DirectX11:	QT_CORE_ASSERT(false, "RendererAPI::Direct11 is currently not supported!"); return nullptr;
		case RendererAPI::API::DirectX12:	QT_CORE_ASSERT(false, "RendererAPI::Direct12 is currently not supported!"); return nullptr;
		}
		QT_CORE_ASSERT(false, "RendererAPI::Unknown RendererAPI");
		return nullptr;
	}

	//-----------------------------------------------------------------------------
	// [API Abstraction] Create Shader with strings
	//-----------------------------------------------------------------------------
	Shader* Shader::Create(const std::string& name, const std::string& vertexSrc, const std::string& fragmentSrc)
	{
		switch (Renderer::GetAPI())
		{
		case RendererAPI::API::None:		QT_CORE_ASSERT(false, "RendererAPI::There is no API Selected"); return nullptr;
		case RendererAPI::API::OpenGL:		return new OpenGLShader(name, vertexSrc, fragmentSrc);
		case RendererAPI::API::Vulkan:		QT_CORE_ASSERT(false, "RendererAPI::Vulkan is currently not supported!"); return nullptr;
		case RendererAPI::API::DirectX11:	QT_CORE_ASSERT(false, "RendererAPI::Direct11 is currently not supported!"); return nullptr;
		case RendererAPI::API::DirectX12:	QT_CORE_ASSERT(false, "RendererAPI::Direct12 is currently not supported!"); return nullptr;
		}
		QT_CORE_ASSERT(false, "RendererAPI::Unknown RendererAPI");
		return nullptr;
	}
}

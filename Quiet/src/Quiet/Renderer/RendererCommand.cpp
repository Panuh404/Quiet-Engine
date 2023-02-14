#include "qtpch.h"

#include "Quiet/Renderer/RendererCommand.h"
#include "Platform/OpenGL/OpenGLRendererAPI.h"

namespace Quiet
{
	std::unique_ptr<RendererAPI> RendererCommand::s_RendererAPI = RendererAPI::Create();
}
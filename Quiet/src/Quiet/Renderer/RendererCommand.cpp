#include "qtpch.h"

#include "Quiet/Renderer/RendererCommand.h"
#include "Platform/OpenGL/OpenGLRendererAPI.h"

namespace Quiet
{
	RendererAPI* RendererCommand::s_RendererAPI = new OpenGLRendererAPI;
}
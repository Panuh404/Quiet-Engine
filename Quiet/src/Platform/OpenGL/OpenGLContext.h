#pragma once

#include "Quiet/Renderer/RendererContext.h"

struct GLFWwindow;

namespace Quiet
{
	//-----------------------------------------------------------------------------
	// [CLASS] Renderer Context -- API: OpenGL
	//-----------------------------------------------------------------------------
	class OpenGLContext : public RendererContext
	{
	public:
		OpenGLContext(GLFWwindow* windowHandle);

		void Init() override;
		void SwapBuffers() override;

	private:
		GLFWwindow* m_WindowHandle;
	};
}

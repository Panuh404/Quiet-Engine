#pragma once

#include "Quiet/Renderer/GraphicsContext.h"

struct GLFWwindow;

namespace Quiet
{
	//-----------------------------------------------------------------------------
	// [CLASS] Renderer Context -- API: OpenGL
	//-----------------------------------------------------------------------------
	class OpenGLContext : public GraphicsContext
	{
	public:
		OpenGLContext(GLFWwindow* windowHandle);

		virtual void Init() override;
		virtual void SwapBuffers() override;
	private:
		GLFWwindow* m_WindowHandle;
	};
}

#include "qtpch.h"

#include "Platform/OpenGL/OpenGLContext.h"
#include "Quiet/Core/Log.h"

#include <GLFW/glfw3.h>
#include <glad/glad.h>
#include <gl/GL.h>

namespace Quiet
{
	OpenGLContext::OpenGLContext(GLFWwindow* windowHandle)
		: m_WindowHandle(windowHandle)
	{
		QT_CORE_ASSERT(windowHandle, "OpenGLContext::Window Handle is null!!")
	}

	void OpenGLContext::Init()
	{
		QT_PROFILE_FUNCTION();

		glfwMakeContextCurrent(m_WindowHandle);
		int status = gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
		QT_CORE_ASSERT(status, "Failed to initialize Glad!");
		
		QT_CORE_INFO("OpenGL::Vendor::{0}", (const char*)glGetString(GL_VENDOR));
		QT_CORE_INFO("OpenGL::Renderer::{0}", (const char*)glGetString(GL_RENDERER));
		QT_CORE_INFO("OpenGL::Version::{0}", (const char*)glGetString(GL_VERSION));

		QT_CORE_ASSERT(GLVersion.major > 4 || (GLVersion.major == 4 && GLVersion.minor >= 5), "Hazel requires at least OpenGL version 4.5!");
	}


	void OpenGLContext::SwapBuffers()
	{
		QT_PROFILE_FUNCTION();
		glfwSwapBuffers(m_WindowHandle);
	}
}
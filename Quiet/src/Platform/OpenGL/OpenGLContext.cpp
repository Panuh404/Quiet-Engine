#include "qtpch.h"

#include "Platform/OpenGL/OpenGLContext.h"
#include "Quiet/Core/Log.h"

#include <GLFW/glfw3.h>
#include <glad/glad.h>
#include <gl/GL.h>

namespace Quiet
{
	OpenGLContext::OpenGLContext(GLFWwindow* windowHandle) : m_WindowHandle(windowHandle)
	{
		QT_CORE_ASSERT(windowHandle, "OpenGLContext::Window Handle is null!!")
	}

	void OpenGLContext::Init()
	{
		// Create a GLFW context
		glfwMakeContextCurrent(m_WindowHandle);

		// Initialize Glad
		int status = gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
		QT_CORE_ASSERT(status, "GLAD::Failed to initialize Glad");

		QT_CORE_INFO("OpenGL::Vendor::{0}", (const char *)glGetString(GL_VENDOR));
		QT_CORE_INFO("OpenGL::Renderer::{0}", (const char*)glGetString(GL_RENDERER));
		QT_CORE_INFO("OpenGL::Version::{0}", (const char*)glGetString(GL_VERSION));
		//QT_CORE_INFO("OpenGL::Extensions::{0}", (const char*)glGetString(GL_EXTENSIONS));
	}

	void OpenGLContext::SwapBuffers()
	{
		glfwSwapBuffers(m_WindowHandle);
	}
}
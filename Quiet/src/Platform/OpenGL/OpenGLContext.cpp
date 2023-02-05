#include "qtpch.h"

#include "Platform/OpenGL/OpenGLContext.h"

#include <GLFW/glfw3.h>
#include <glad/glad.h>

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
		QT_CORE_ASSERT(status, "GLAD::Failed to initialize Glad")

		
	}

	void OpenGLContext::SwapBuffers()
	{
		glfwSwapBuffers(m_WindowHandle);
	}

}
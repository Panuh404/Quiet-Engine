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
		QT_PROFILE_FUNCTION();
		// Create a GLFW context
		glfwMakeContextCurrent(m_WindowHandle);

		// Initialize Glad
		int status = gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
		QT_CORE_ASSERT(status, "GLAD::Failed to initialize Glad");

		QT_CORE_INFO("OpenGL::Vendor::{0}", (const char *)glGetString(GL_VENDOR));
		QT_CORE_INFO("OpenGL::Renderer::{0}", (const char*)glGetString(GL_RENDERER));
		QT_CORE_INFO("OpenGL::Version::{0}", (const char*)glGetString(GL_VERSION));
		//QT_CORE_INFO("OpenGL::Extensions::{0}", (const char*)glGetString(GL_EXTENSIONS));

		#ifdef QT_ENABLE_ASSERTS
			int versionMajor;
			int versionMinor;
			glGetIntegerv(GL_MAJOR_VERSION, &versionMajor);
			glGetIntegerv(GL_MINOR_VERSION, &versionMinor);

			QT_CORE_ASSERT(versionMajor > 4 || (versionMajor == 4 && versionMinor >= 5), "Quiet requires at least OpenGL version 4.5!");
		#endif
			
	}

	void OpenGLContext::SwapBuffers()
	{
		QT_PROFILE_FUNCTION();
		glfwSwapBuffers(m_WindowHandle);
	}
}
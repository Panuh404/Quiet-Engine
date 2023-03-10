#include "qtpch.h"

#include "Quiet/Core/Application.h"
#include "Quiet/Core/Log.h"
#include "Quiet/Core/Timestep.h"
#include "Quiet/Renderer/Renderer.h"

#include "GLFW/glfw3.h"

namespace Quiet
{
	Application* Application::s_Instance = nullptr;

	Application::Application()
	{
		QT_PROFILE_FUNCTION();
		// Create Application Instance
		QT_CORE_ASSERT(!s_Instance, "Application already exists!");
		s_Instance = this;

		// Create Window
		m_Window = Window::Create();
		m_Window->SetEventCallback(BIND_EVENT_FN(Application::OnEvent));
		m_Window->SetVSync(true);

		// Rendering
		Renderer::Init();

		// Create ImGui Layer
		m_ImGuiLayer = new ImGuiLayer();
		PushOverlay(m_ImGuiLayer);		
	}

	Application::~Application()
	{
		QT_PROFILE_FUNCTION();
		Renderer::Shutdown();
	}

	void Application::PushLayer(Layer* layer)
	{
		QT_PROFILE_FUNCTION();
		m_LayerStack.PushLayer(layer);
		layer->OnAttach();
	}

	void Application::PushOverlay(Layer* overlay)
	{
		QT_PROFILE_FUNCTION();
		m_LayerStack.PushOverlay(overlay);
		overlay->OnAttach();
	}

	void Application::OnEvent(Event& e)
	{
		QT_PROFILE_FUNCTION();
		EventDispatcher dispatcher(e);
		dispatcher.Dispatch<WindowCloseEvent>(BIND_EVENT_FN(Application::OnWindowClose));
		dispatcher.Dispatch<WindowResizeEvent>(BIND_EVENT_FN(Application::OnWindowResize));
		
		for (auto it = m_LayerStack.rbegin(); it != m_LayerStack.rend(); ++it)
		{
			(*it)->OnEvent(e);
			if (e.Handled)
				break;
		}
	}

	void Application::Run()
	{
		QT_PROFILE_FUNCTION();
		while (m_Running)
		{
			QT_PROFILE_SCOPE("RunLoop");
			// Timestep
			float time = (float)glfwGetTime();
			Timestep timestep = time - m_LastFrameTime;
			m_LastFrameTime = time;
			
			if(!m_Minimized)
			{
				{
					QT_PROFILE_SCOPE("LayerStack OnUpdate");
					for (Layer* layer : m_LayerStack) { layer->OnUpdate(timestep); }
				}
			}

			// ImGui Render
			m_ImGuiLayer->Begin();
			{
				QT_PROFILE_SCOPE("LayerStack OnImGuiRender");
				for (Layer* layer : m_LayerStack) { layer->OnImGuiRender(); }
			}
			m_ImGuiLayer->End();

			// Window
			m_Window->OnUpdate();
		}
	}

	bool Application::OnWindowClose(WindowCloseEvent& e)
	{
		m_Running = false;
		return true;
	}

	bool Application::OnWindowResize(WindowResizeEvent& e)
	{
		QT_PROFILE_FUNCTION();

		if(e.GetWidth() == 0 || e.GetHeight() == 0)
		{
			m_Minimized = true;
			return false;
		}
		m_Minimized = false;
		Renderer::OnWindowResize(e.GetWidth(), e.GetHeight());
		return false;
	}

}

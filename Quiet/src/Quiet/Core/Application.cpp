#include "qtpch.h"

#include "Quiet/Core/Application.h"
#include "Quiet/Core/Log.h"
#include "Quiet/Core/Input.h"

#include "glm/glm.hpp"

#include <glad/glad.h>

namespace Quiet
{
	Application* Application::s_Instance = nullptr;

	Application::Application()
	{
		// Create Application Instance
		QT_CORE_ASSERT(!s_Instance, "Application already exists!!!")
		s_Instance = this;

		// Create Window
		m_Window = std::unique_ptr<Window>(Window::Create());
		m_Window->SetEventCallback(BIND_EVENT_FN(Application::OnEvent));

		// Create ImGui Layer
		m_ImGuiLayer = new ImGuiLayer();
		PushOverlay(m_ImGuiLayer);
	}

	Application::~Application()
	{

	}

	void Application::PushLayer(Layer* layer)
	{
		m_LayerStack.PushLayer(layer);
		layer->OnAttach();
	}

	void Application::PushOverlay(Layer* overlay)
	{
		m_LayerStack.PushOverlay(overlay);
		overlay->OnAttach();
	}

	void Application::OnEvent(Event& e)
	{
		EventDispatcher dispatcher(e);
		dispatcher.Dispatch<WindowCloseEvent>(BIND_EVENT_FN(Application::OnWindowClose));
		
		for (auto it = m_LayerStack.end(); it != m_LayerStack.begin(); )
		{
			(*--it)->OnEvent(e);
			if (e.Handled)
				break;
		}
	}

	void Application::Run()
	{
		while (m_Running)
		{
			glClearColor(0.2f, 0.3f, 0.7f, 1.0f);
			glClear(GL_COLOR_BUFFER_BIT);

			// Layers
			for (Layer* layer : m_LayerStack)
			{
				layer->OnUpdate();
			}

			// ImGui Render
			m_ImGuiLayer->Begin();
			for (Layer* layer : m_LayerStack)
			{
				layer->OnImGuiRender();
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
}

#pragma once

#include "Quiet/Core/Core.h"

#include "Quiet/Core/Window.h"
#include "Quiet/Core/LayerStack.h"
#include "Quiet/Events/Event.h"
#include "Quiet/Events/ApplicationEvent.h"

#include "Quiet/ImGui/ImGuiLayer.h"

namespace Quiet
{
	class QUIET_API Application
	{
	public:
		Application();
		virtual ~Application();

		void Run();
		void OnEvent(Event& e);

		void PushLayer(Layer* layer);
		void PushOverlay(Layer* overlay);

		inline static Application& Get() { return *s_Instance; }
		inline Window& GetWindow() { return *m_Window; }

	private:
		bool OnWindowClose(WindowCloseEvent& e);
		bool OnWindowResize(WindowResizeEvent& e);

		static Application* s_Instance;
		std::unique_ptr<Window> m_Window;
		ImGuiLayer* m_ImGuiLayer;
		LayerStack m_LayerStack;

		uint32_t m_VertexArray;
		uint32_t m_VertexBuffer;
		uint32_t m_IndexBuffer;

		bool m_Running = true;
		bool m_Minimized = false;

	};

	// to be defined in client
	Application* CreateApplication();
}
#pragma once

#include "Quiet/Core/Window.h"
#include "Quiet/Renderer/GraphicsContext.h"

#include <GLFW/glfw3.h>

namespace Quiet
{
	//-----------------------------------------------------------------------------
	// [CLASS] Window -- Platform: Windows
	//-----------------------------------------------------------------------------
	class WindowsWindow : public Window
	{
	public:
		WindowsWindow(const WindowProps& props);
		virtual ~WindowsWindow();

		void OnUpdate() override;

		unsigned int GetWidth() const override { return m_Data.Width; }
		unsigned int GetHeight() const override { return m_Data.Height; }

		// Window attributes
		void SetEventCallback(const EventCallbackFn& callback) override { m_Data.EventCallback = callback; }
		void SetVSync(bool enabled) override;
		bool IsVSync() const override;

		virtual void* GetNativeWindow() const { return m_Window; }

	private:
		GLFWwindow* m_Window;
		std::unique_ptr<GraphicsContext> m_Context;

		struct WindowData
		{
			std::string Title;
			uint32_t Width, Height;
			bool VSync;

			EventCallbackFn EventCallback;
		};
		WindowData m_Data;

		virtual void Init(const WindowProps& props);
		virtual void Shutdown();
	};
}
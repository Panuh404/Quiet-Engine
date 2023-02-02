#include <Quiet.h>

class ExampleLayer : public Quiet::Layer
{
public:
	ExampleLayer() : Layer("Example")
	{		
	}

	void OnUpdate() override
	{
		if(Quiet::Input::IsKeyPressed(Quiet::Key::Tab))
		{
			QT_TRACE("Tab Key is Pressed!");
		}
	}

	void OnEvent(Quiet::Event& event) override
	{
		if(event.GetEventType() == Quiet::EventType::KeyPressed)
		{
			Quiet::KeyPressedEvent& e = (Quiet::KeyPressedEvent&)event;
			QT_TRACE("{0}", e.GetKeyCode());
		}
	}
};

class Sandbox : public Quiet::Application
{
public:
	Sandbox()
	{
		PushLayer(new ExampleLayer());
		PushOverlay(new Quiet::ImGuiLayer());
	}

	~Sandbox()
	{
		
	}
};

Quiet::Application* Quiet::CreateApplication()
{
	return new Sandbox();
}

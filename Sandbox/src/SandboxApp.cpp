#include <Quiet.h>

class ExampleLayer : public Quiet::Layer
{
public:
	ExampleLayer() : Layer("Example")
	{		
	}

	void OnUpdate() override
	{
	}

	void OnEvent(Quiet::Event& event) override
	{
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

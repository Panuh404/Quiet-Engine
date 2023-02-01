#include <Quiet.h>

class ExampleLayer : public Quiet::Layer
{
public:
	ExampleLayer() : Layer("Example")
	{		
	}

	void OnUpdate() override
	{
		//QT_INFO("ExampleLayer::Update");
	}

	void OnEvent(Quiet::Event& event) override
	{
		//QT_TRACE("ExampleLayer::Event::{0}", event);
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

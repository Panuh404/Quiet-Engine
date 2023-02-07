#include <Quiet.h>

#include "ImGui/imgui.h"

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

	void OnImGuiRender() override
	{
		ImGui::Begin("Dialog");
		ImGui::Text("Vertex Buffer Test");
		ImGui::End();

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
	}

	~Sandbox()
	{
		
	}
};

Quiet::Application* Quiet::CreateApplication()
{
	return new Sandbox();
}

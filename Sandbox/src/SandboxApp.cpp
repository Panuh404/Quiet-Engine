#include <Quiet.h>
#include "Quiet/Core/EntryPoint.h"

// Scenes
#include "Sandbox2D.h"
#include "ExampleLayer.h"

class Sandbox : public Quiet::Application
{
public:
	Sandbox()
	{
		//PushLayer(new ExampleLayer());
		PushLayer(new Sandbox2D());
	}

	~Sandbox()
	{
		
	}
};

Quiet::Application* Quiet::CreateApplication()
{
	return new Sandbox();
}

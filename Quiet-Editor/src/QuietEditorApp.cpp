#include <Quiet.h>
#include "Quiet/Core/EntryPoint.h"

// Scenes
#include "EditorLayer.h"

namespace Quiet
{
	class QuietEditor : public Application
	{
	public:
		QuietEditor() : Application("Quiet Editor")
		{
			//PushLayer(new ExampleLayer());
			PushLayer(new EditorLayer());
		}

		~QuietEditor()
		{
			
		}
	};

	Application* CreateApplication()
	{
		return new QuietEditor();
	}
}

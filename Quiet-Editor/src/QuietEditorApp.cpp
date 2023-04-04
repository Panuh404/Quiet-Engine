#include <Quiet.h>
#include "Quiet/Core/EntryPoint.h"

#include "EditorLayer.h"

namespace Quiet
{
	class QuietEditor : public Application
	{
	public:
		QuietEditor() : Application("Quiet Editor")
		{
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

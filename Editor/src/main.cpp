#include "Quiet/Core/Application.h"

int main()
{
    Quiet::Core::AppSpec spec{};
    spec.Name = "Quiet Editor";
    spec.Mode = Quiet::Core::AppMode::Editor;

    Quiet::Core::Application app(spec);
    return app.Run();
}
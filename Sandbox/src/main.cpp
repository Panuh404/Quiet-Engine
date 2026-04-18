#include "Quiet/Core/Application.h"

int main()
{
    Quiet::Core::AppSpec spec{};
    spec.Name = "Quiet Sandbox";
    spec.Width = 1200;
    spec.Height = 800;
    spec.Mode = Quiet::Core::AppMode::Sandbox;

    Quiet::Core::Application app(spec);
    return app.Run();
}
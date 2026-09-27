#include "Quiet/Core/Application.h"

int main(const int argc, char** argv)
{
    Quiet::Core::AppSpec spec{};

    spec.Name = "Quiet Sandbox";
    spec.Width = 1200;
    spec.Height = 800;
    spec.Mode = Quiet::Core::AppMode::Sandbox;

    if (argc >= 2)
    {
        spec.RuntimePackagePath = argv[1];
    }

    Quiet::Core::Application app(spec);
    return app.Run();
}
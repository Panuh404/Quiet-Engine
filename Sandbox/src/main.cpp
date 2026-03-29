#include "Quiet/Core/Application.h"

int main()
{
    Quiet::Core::AppSpec spec{};
    spec.Name = "Quiet Sandbox";
    spec.Width = 1920;
    spec.Height = 1080;

    Quiet::Core::Application app(spec);
    return app.Run();
}
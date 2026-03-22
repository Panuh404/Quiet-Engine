#include "Quiet/Core/Application.h"

int main()
{
    Quiet::Core::AppSpec spec{};
    spec.Name = "Quiet Sandbox";
    spec.Width = 1600;
    spec.Height = 900;

    Quiet::Core::Application app(spec);
    return app.Run();
}
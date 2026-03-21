#include "Quiet/Core/Application.h"

int main()
{
    Quiet::Core::AppSpec spec{};
    spec.Name = "Quiet Sandbox";

    Quiet::Core::Application app(spec);
    return app.Run();
}
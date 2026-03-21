#include "Quiet/Core/Application.h"

// TEMPORAL ENTRYPOINT
int main()
{
    Quiet::Core::AppSpec spec{};
    spec.Name = "Quiet Editor";

    Quiet::Core::Application app(spec);
    return app.Run();
}
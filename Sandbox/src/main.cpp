#include <exception>
#include <iostream>

#include "Quiet/Core/Application.h"
#include "Quiet/Core/Log.h"

int main(const int argc, char** argv)
{
    try
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
    catch (const std::exception& exception)
    {
        std::cerr << "Quiet Sandbox failed: " << exception.what() << '\n';
        Quiet::Core::Log::Shutdown();
        return 1;
    }
    catch (...)
    {
        std::cerr << "Quiet Sandbox failed with an unknown error.\n";
        Quiet::Core::Log::Shutdown();
        return 1;
    }
}
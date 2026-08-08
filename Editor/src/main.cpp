#include <filesystem>
#include "Quiet/Core/Application.h"

int main(int argc, char** argv)
{
    Quiet::Core::AppSpec spec{};
    spec.Name = "Quiet Editor";
    spec.WindowIconPath = "Resources/Textures/Editor/quiet-icon.png";
    spec.Mode = Quiet::Core::AppMode::Editor;

    if (argc > 1 && argv[1] && argv[1][0] != '\0')
    {
        spec.ProjectFilePath = std::filesystem::absolute(argv[1]).lexically_normal();
    }

    Quiet::Core::Application app(spec);
    return app.Run();
}
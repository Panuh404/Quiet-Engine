#pragma once

#include <string>

namespace Quiet::Core
{
    struct AppSpec
    {
        std::string Name = "Quiet Application";
    };

    class Application
    {
    public:
        explicit Application(AppSpec spec);
        int Run();

    private:
        AppSpec m_Spec;
        bool m_Running = true;
    };
}
#include "Quiet/Core/Application.h"

#include <iostream>
#include <utility>

namespace Quiet::Core
{
    Application::Application(AppSpec spec)
        : m_Spec(std::move(spec))
    {
    }

    int Application::Run()
    {
        std::cout << "[Quiet] Starting application: " << m_Spec.Name << '\n';
        std::cout << "[Quiet] Engine skeleton initialized successfully.\n";
        std::cout << "[Quiet] Shutting down.\n";

        return 0;
    }
}
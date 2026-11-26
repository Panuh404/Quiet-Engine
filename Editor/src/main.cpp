#include <filesystem>
#include "Quiet/Core/Application.h"

#include "Quiet/UI/Widgets/Button.h"
#include "Quiet/UI/Widgets/Label.h"

int main(int argc, char** argv)
{
    Quiet::Core::AppSpec spec{};
    spec.Name = "Quiet Editor";
    spec.WindowIconPath = "Resources/Textures/Editor/quiet-icon.png";
    spec.Mode = Quiet::Core::AppMode::Editor;

    spec.BindEditorPlayHUD = [](Quiet::Core::UUID documentID, Quiet::UI::Widget& root, std::string& error)
    {
        constexpr Quiet::Core::UUID fixtureDocument{0xce4e18a5106b43d2ULL, 0xa901000000000001ULL};
        constexpr Quiet::Core::UUID statusNode{0xce4e18a5106b43d2ULL, 0xb201000000000001ULL};
        constexpr Quiet::Core::UUID buttonNode{0xce4e18a5106b43d2ULL, 0xb201000000000003ULL};

        if (documentID != fixtureDocument) return true;

        auto* label = dynamic_cast<Quiet::UI::Label*>(root.FindByID(statusNode));
        auto* button = dynamic_cast<Quiet::UI::Button*>(root.FindByID(buttonNode));
        if (!label || !button)
        {
            error = "Expected Label node ce4e18a5106b43d2-b201000000000001 and "
                    "Button node ce4e18a5106b43d2-b201000000000003.";
            return false;
        }

        button->SetOnClick([label] { label->SetContent("Activated"); });
        return true;
    };

    if (argc > 1 && argv[1] && argv[1][0] != '\0')
    {
        spec.ProjectFilePath = std::filesystem::absolute(argv[1]).lexically_normal();
    }

    Quiet::Core::Application app(spec);
    return app.Run();
}

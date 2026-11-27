#pragma once

#include <string>

#include "Quiet/Core/UUID.h"
#include "Quiet/UI/Widgets/Button.h"
#include "Quiet/UI/Widgets/Label.h"

namespace Quiet::Samples
{
    inline bool BindTestDriveHUD(Core::UUID documentID, UI::Widget& root, std::string& error)
    {
        constexpr Core::UUID fixtureDocument{0xce4e18a5106b43d2ULL, 0xa901000000000001ULL};
        constexpr Core::UUID statusNode{0xce4e18a5106b43d2ULL, 0xb201000000000001ULL};
        constexpr Core::UUID buttonNode{0xce4e18a5106b43d2ULL, 0xb201000000000003ULL};

        if (documentID != fixtureDocument) return true;

        auto* label = dynamic_cast<UI::Label*>(root.FindByID(statusNode));
        auto* button = dynamic_cast<UI::Button*>(root.FindByID(buttonNode));
        if (!label || !button)
        {
            error = "Expected Label node " + Core::UUIDToString(statusNode) +
                    " and Button node " + Core::UUIDToString(buttonNode) + ".";
            return false;
        }

        button->SetOnClick([label] { label->SetContent("Activated"); });
        return true;
    }
}

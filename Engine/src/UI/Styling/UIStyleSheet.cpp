#include "Quiet/UI/Styling/UIStyleSheet.h"

#include <array>
#include <cmath>
#include <unordered_set>

namespace Quiet::UI
{
    bool ValidateUIStyleSheet(const UIStyleSheet& sheet, std::string& outError)
    {
        outError.clear();
        std::unordered_set<Core::UUID> ids;

        const auto finiteColor = [](const glm::vec4& color)
        {
            return std::isfinite(color.r) &&
                   std::isfinite(color.g) &&
                   std::isfinite(color.b) &&
                   std::isfinite(color.a);
        };

        for (const UIStyleEntry& entry : sheet.Styles)
        {
            if (!entry.ID.IsValid())
            {
                outError = "UI style has an invalid UUID.";
                return false;
            }

            if (!ids.insert(entry.ID).second)
            {
                outError = "UI style has a duplicate UUID: " + Core::UUIDToString(entry.ID);
                return false;
            }

            if (entry.Properties.valueless_by_exception())
            {
                outError = "UI style has no properties: " + Core::UUIDToString(entry.ID);
                return false;
            }

            if (const auto* style = std::get_if<ButtonStyle>(&entry.Properties))
            {
                const glm::vec4& padding = style->Padding;

                if (!std::isfinite(padding.x) ||
                    !std::isfinite(padding.y) ||
                    !std::isfinite(padding.z) ||
                    !std::isfinite(padding.w) ||
                    padding.x < 0.0f ||
                    padding.y < 0.0f ||
                    padding.z < 0.0f ||
                    padding.w < 0.0f)
                {
                    outError = "Invalid button padding in UI style: " + Core::UUIDToString(entry.ID);
                    return false;
                }

                const std::array colors
                {
                    &style->NormalColor,
                    &style->HoveredColor,
                    &style->PressedColor,
                    &style->DisabledColor,
                    &style->FocusedColor
                };

                for (const glm::vec4* color : colors)
                {
                    if (!finiteColor(*color))
                    {
                        outError = "Invalid button color in UI style: " + Core::UUIDToString(entry.ID);
                        return false;
                    }
                }
            }

            if (const auto* style = std::get_if<TextStyle>(&entry.Properties))
            {
                if (!std::isfinite(style->PixelHeight) ||
                    style->PixelHeight <= 0.0f ||
                    !finiteColor(style->Color))
                {
                    outError = "Invalid text style: " + Core::UUIDToString(entry.ID);
                    return false;
                }
            }
        }
        return true;
    }
}
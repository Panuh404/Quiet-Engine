#pragma once

#include <string>
#include <variant>
#include <vector>

#include "Quiet/Core/UUID.h"
#include "Quiet/UI/Styling/ButtonStyle.h"
#include "Quiet/UI/Styling/TextStyle.h"

namespace Quiet::UI
{
    using UIStyleProperties = std::variant<ButtonStyle, TextStyle>;

    /**
     * @brief Describes one reusable, named style.
     * @ingroup quiet_renderer
     *
     * ID is intended to remain stable when Name changes. A default UUID is
     * invalid. Validation and style application are not implemented yet.
     */
    struct UIStyleEntry
    {
        Core::UUID ID{};
        std::string Name;
        UIStyleProperties Properties = ButtonStyle{};
    };

    /**
     * @brief Owns a collection of reusable UI style values.
     * @ingroup quiet_renderer
     *
     * This in-memory description owns no widgets or rendering resources.
     * Persistence, validation, and application are future steps.
     */
    struct UIStyleSheet
    {
        std::vector<UIStyleEntry> Styles;
    };

    /**
     * @brief Validates style identities and values.
     * @return True if the sheet is valid; otherwise false with an error message.
     * @note Empty sheets are valid. Names need not be unique.
     *       Clears outError before validation.
     * @pre outError must not refer to a string owned by sheet.
     */
    [[nodiscard]] bool ValidateUIStyleSheet(const UIStyleSheet& sheet, std::string& outError);
}
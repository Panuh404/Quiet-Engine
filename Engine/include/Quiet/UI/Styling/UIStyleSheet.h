#pragma once

#include <string>
#include <variant>
#include <vector>
#include <filesystem>

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

    /**
     * @brief Serializes a validated UI style sheet to versioned JSON text.
     * @return True on success; otherwise false with an error message.
     * @note On reported failure, outJSON is unchanged.
     *       Allocation exceptions may propagate.
     * @pre outJSON and outError must be distinct and must not refer to strings
     *      owned by sheet.
     */
    [[nodiscard]] bool SerializeUIStyleSheet(const UIStyleSheet& sheet, std::string& outJSON, std::string& outError);

    /**
     * @brief Reads a versioned UI style sheet from JSON text.
     * @return True on success; otherwise false with an error message.
     * @note On reported failure, outSheet is unchanged.
     *       Allocation exceptions may propagate.
     * @pre outError must not refer to jsonText or to a string owned by outSheet.
     */
    [[nodiscard]] bool DeserializeUIStyleSheet(const std::string& jsonText, UIStyleSheet& outSheet, std::string& outError);

    /**
     * @brief Saves a UI style sheet as JSON.
     * @return True on success; otherwise false with an error message.
     * @note Validation and serialization happen before opening the file.
     *       Does not create parent directories or provide atomic replacement.
     *       A write failure can leave an incomplete file.
     * @pre outError must not refer to a string owned by sheet.
     */
    [[nodiscard]] bool SaveUIStyleSheetToFile(const std::filesystem::path& path, const UIStyleSheet& sheet, std::string& outError);

    /**
     * @brief Loads a UI style sheet from a JSON file.
     * @return True on success; otherwise false with an error message.
     * @note On reported failure, outSheet is unchanged.
     * @pre outError must not refer to a string owned by outSheet.
     */
    [[nodiscard]] bool LoadUIStyleSheetFromFile(const std::filesystem::path& path, UIStyleSheet& outSheet, std::string& outError);
}
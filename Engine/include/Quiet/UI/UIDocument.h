#pragma once

#include <optional>
#include <memory>
#include <string>
#include <variant>
#include <vector>
#include <filesystem>

#include <glm/vec2.hpp>
#include <glm/vec4.hpp>

#include "Quiet/Core/UUID.h"
#include "Quiet/UI/Styling/UIStyleSheet.h"
#include "Quiet/UI/Geometry.h"
#include "Quiet/UI/Widget.h"

namespace Quiet::UI
{
    // Authored properties specific to each existing widget kind.

    struct UIDocumentWidget {};

    struct UIDocumentPanel {};

    struct UIDocumentLabel
    {
        std::string Content;
    };

    struct UIDocumentImage
    {
        std::string TextureID;
    };

    struct UIDocumentButton {};

    struct UIDocumentHorizontalBox
    {
        glm::vec4 Padding{0.0f};
        float Spacing = 0.0f;
    };

    struct UIDocumentVerticalBox
    {
        glm::vec4 Padding{0.0f};
        float Spacing = 0.0f;
    };

    using UIDocumentWidgetProperties = std::variant<
        UIDocumentWidget,
        UIDocumentPanel,
        UIDocumentLabel,
        UIDocumentImage,
        UIDocumentButton,
        UIDocumentHorizontalBox,
        UIDocumentVerticalBox
    >;

    /**
     * @brief Describes one authored widget and its ordered children.
     * @ingroup quiet_renderer
     *
     * ID is a stable document-local UUID. Name is a human-readable label and
     * may be empty or duplicated. Renaming does not change the UUID.
     *
     * Focusable and HitTest optionally override the corresponding runtime
     * widget defaults. An absent value retains the constructed widget's default.
     *
     * Contains no live widgets, callbacks, or rendering resources.
     */
    struct UIDocumentNode
    {
        Core::UUID ID{};
        std::string Name;
        std::optional<Core::UUID> StyleID;

        Rect Bounds{};
        glm::vec2 Anchor{0.0f};
        glm::vec2 Pivot{0.0f};
        Alignment HorizontalAlignment = Alignment::Stretch;
        Alignment VerticalAlignment = Alignment::Stretch;

        bool Enabled = true;
        bool ClipToBounds = false;
        std::optional<bool> Focusable;
        std::optional<HitTestMode> HitTest;

        UIDocumentWidgetProperties Properties = UIDocumentWidget{};
        std::vector<UIDocumentNode> Children;
    };

    /**
     * @brief Owns an ordered collection of authored UI root nodes.
     * @ingroup quiet_renderer
     *
     * The document supports ID and property validation and can build an
     * independent runtime widget tree. Persistence, styling, and behavior
     * binding remain future work.
     */
    struct UIDocument
    {
        std::vector<UIDocumentNode> Roots;
    };

    #pragma region Validations

    /**
     * @brief Checks that every node has a valid, document-local unique UUID.
     * @return True if all IDs are valid; otherwise false with an error message.
     * @note An empty document is valid. Clears outError before validation.
     * @pre outError must not refer to a string owned by document.
     */
    [[nodiscard]] bool ValidateUIDocumentNodeIDs(const UIDocument& document, std::string& outError);

    /**
     * @brief Checks authored geometry, enum values, and box layout properties.
     * @return True if the checked properties are valid; otherwise false.
     * @note Clears outError before validation and reports the first invalid node.
     *       Does not check IDs, text, or referenced resources.
     * @pre outError must not refer to a string owned by document.
     */
    [[nodiscard]] bool ValidateUIDocumentProperties(const UIDocument& document, std::string& outError);

    #pragma endregion

    #pragma region Serialization/Deserialization

    /**
     * @brief Serializes a validated UI document to versioned JSON text.
     * @return True on success; otherwise false with an error message.
     * @note On reported failure, outJSON is unchanged.
     *       Style references are recorded but not resolved.
     *       Allocation exceptions may propagate.
     * @pre outJSON and outError must be distinct and must not refer to strings
     *      owned by document.
     */
    [[nodiscard]] bool SerializeUIDocument(const UIDocument& document, std::string& outJSON, std::string& outError);

    /**
     * @brief Reads a versioned UI document from JSON text.
     * @return True on success; otherwise false with an error message.
     * @note On reported failure, outDocument is unchanged.
     *       Style references are read but not resolved.
     *       Allocation exceptions may propagate.
     * @pre outError must not refer to jsonText or to a string owned by outDocument.
     */
    [[nodiscard]] bool DeserializeUIDocument(const std::string& jsonText, UIDocument& outDocument, std::string& outError);

    /**
     * @brief Saves a UI document as JSON.
     * @return True on success; otherwise false with an error message.
     * @note Validation and serialization happen before the file is opened.
     *       Does not create parent directories or provide atomic replacement.
     *       A write failure can leave an incomplete file.
     * @pre outError must not refer to a string owned by document.
     */
    [[nodiscard]] bool SaveUIDocumentToFile(const std::filesystem::path& path, const UIDocument& document, std::string& outError);

    /**
     * @brief Loads a UI document from a JSON file.
     * @return True on success; otherwise false with an error message.
     * @note On reported failure, outDocument is unchanged.
     * @pre outError must not refer to a string owned by outDocument.
     */
    [[nodiscard]] bool LoadUIDocumentFromFile(const std::filesystem::path& path, UIDocument& outDocument, std::string& outError);

    #pragma endregion

    #pragma region Build

    /**
     * @brief Builds an independently owned runtime widget tree without styles.
     * @return An owning root, or null when validation fails.
     * @note A document with a style reference requires the overload taking a sheet.
     * @pre outError must not refer to a string owned by document.
     */
    [[nodiscard]] std::unique_ptr<Widget> BuildUIDocument(const UIDocument& document, std::string& outError);

    /**
     * @brief Builds an independently owned runtime widget tree with styles.
     * @return An owning root, or null when validation fails.
     * @note Validates the document, sheet, and style references before construction.
     *       The sheet is borrowed only for this call. Widgets receive style copies.
     *       Button callbacks remain empty. Exceptions propagate.
     * @pre outError must not refer to a string owned by document or sheet.
     */
    [[nodiscard]] std::unique_ptr<Widget> BuildUIDocument(const UIDocument& document, const UIStyleSheet& sheet, std::string& outError);

    #pragma endregion
}
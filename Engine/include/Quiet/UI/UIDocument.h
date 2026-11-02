#pragma once

#include <optional>
#include <memory>
#include <string>
#include <variant>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec4.hpp>

#include "Quiet/Core/UUID.h"
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

    /**
     * @brief Builds an independently owned runtime widget tree.
     * @return An owning root, or null when document validation fails.
     * @note Validates IDs and checked properties before constructing widgets.
     *       The returned root owns the document roots in their authored order.
     *       Button callbacks are empty and widget appearance uses runtime defaults.
     *       Allocation and widget-construction exceptions propagate.
     * @pre outError must not refer to a string owned by document.
     */
    [[nodiscard]] std::unique_ptr<Widget> BuildUIDocument(const UIDocument& document, std::string& outError);
}
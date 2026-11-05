#include "Quiet/UI/UIDocument.h"

#include <nlohmann/json.hpp>

#include "Quiet/UI/Layout/HorizontalBox.h"
#include "Quiet/UI/Layout/VerticalBox.h"
#include "Quiet/UI/Widgets/Button.h"
#include "Quiet/UI/Widgets/Image.h"
#include "Quiet/UI/Widgets/Label.h"
#include "Quiet/UI/Widgets/Panel.h"

#include <algorithm>
#include <memory>
#include <utility>
#include <cmath>
#include <unordered_set>
#include <stdexcept>
#include <fstream>
#include <array>

namespace Quiet::UI
{
#pragma region Validations

    bool ValidateUIDocumentNodeIDs(const UIDocument& document, std::string& outError)
    {
        outError.clear();
        std::unordered_set<Core::UUID> ids;

        const auto validateNode = [&ids, &outError](const auto& visit, const UIDocumentNode& node) -> bool
        {
            if (!node.ID.IsValid())
            {
                outError = "UI document contains a node with an invalid UUID.";
                return false;
            }

            if (!ids.insert(node.ID).second)
            {
                outError = "UI document contains a duplicate node UUID: " + Core::UUIDToString(node.ID);
                return false;
            }

            for (const UIDocumentNode& child : node.Children)
            {
                if (!visit(visit, child)) return false;
            }

            return true;
        };

        for (const UIDocumentNode& root : document.Roots)
        {
            if (!validateNode(validateNode, root)) return false;
        }

        return true;
    }

    bool ValidateUIDocumentProperties(const UIDocument& document, std::string& outError)
    {
        outError.clear();

        const auto validAlignment = [](const Alignment alignment)
        {
            switch (alignment)
            {
                case Alignment::Start:
                case Alignment::Center:
                case Alignment::End:
                case Alignment::Stretch:
                    return true;
            }
            return false;
        };

        const auto validHitTest = [](const HitTestMode mode)
        {
            switch (mode)
            {
                case HitTestMode::SelfAndChildren:
                case HitTestMode::ChildrenOnly:
                case HitTestMode::None:
                    return true;
            }
            return false;
        };

        const auto validPadding = [](const glm::vec4& padding)
        {
            return std::isfinite(padding.x) &&
                   std::isfinite(padding.y) &&
                   std::isfinite(padding.z) &&
                   std::isfinite(padding.w) &&
                   padding.x >= 0.0f &&
                   padding.y >= 0.0f &&
                   padding.z >= 0.0f &&
                   padding.w >= 0.0f;
        };

        const auto validateNode = [&](const auto& visit, const UIDocumentNode& node) -> bool
        {
            const Rect& bounds = node.Bounds;

            if (!std::isfinite(bounds.Position.x) ||
                !std::isfinite(bounds.Position.y) ||
                !std::isfinite(bounds.Size.x) ||
                !std::isfinite(bounds.Size.y) ||
                bounds.Size.x < 0.0f ||
                bounds.Size.y < 0.0f)
            {
                outError = "Invalid bounds on UI node: " + Core::UUIDToString(node.ID);
                return false;
            }

            if (!std::isfinite(node.Anchor.x) ||
                !std::isfinite(node.Anchor.y) ||
                !std::isfinite(node.Pivot.x) ||
                !std::isfinite(node.Pivot.y) ||
                node.Anchor.x < 0.0f || node.Anchor.x > 1.0f ||
                node.Anchor.y < 0.0f || node.Anchor.y > 1.0f ||
                node.Pivot.x < 0.0f || node.Pivot.x > 1.0f ||
                node.Pivot.y < 0.0f || node.Pivot.y > 1.0f)
            {
                outError = "Invalid anchor or pivot on UI node: " + Core::UUIDToString(node.ID);
                return false;
            }

            if (!validAlignment(node.HorizontalAlignment) ||
                !validAlignment(node.VerticalAlignment) ||
                (node.HitTest && !validHitTest(*node.HitTest)))
            {
                outError = "Invalid alignment or hit-test mode on UI node: " + Core::UUIDToString(node.ID);
                return false;
            }

            if (node.Properties.valueless_by_exception())
            {
                outError = "Missing widget properties on UI node: " + Core::UUIDToString(node.ID);
                return false;
            }

            if (const auto* box = std::get_if<UIDocumentHorizontalBox>(&node.Properties))
            {
                if (!validPadding(box->Padding) ||
                    !std::isfinite(box->Spacing) ||
                    box->Spacing < 0.0f)
                {
                    outError = "Invalid horizontal box layout on UI node: " + Core::UUIDToString(node.ID);
                    return false;
                }
            }

            if (const auto* box = std::get_if<UIDocumentVerticalBox>(&node.Properties))
            {
                if (!validPadding(box->Padding) ||
                    !std::isfinite(box->Spacing) ||
                    box->Spacing < 0.0f)
                {
                    outError = "Invalid vertical box layout on UI node: " + Core::UUIDToString(node.ID);
                    return false;
                }
            }

            for (const UIDocumentNode& child : node.Children)
            {
                if (!visit(visit, child)) return false;
            }
            return true;
        };

        for (const UIDocumentNode& root : document.Roots)
        {
            if (!validateNode(validateNode, root)) return false;
        }
        return true;
    }

#pragma endregion

#pragma region Serialization/Deserialization

    bool SerializeUIDocument(const UIDocument& document, std::string& outJSON, std::string& outError)
    {
        if (!ValidateUIDocumentNodeIDs(document, outError) ||
            !ValidateUIDocumentProperties(document, outError))
        {
            return false;
        }

        // A reference is optional, but a present UUID must be valid.
        const auto validateStyleIDs = [&outError](const auto& visit, const UIDocumentNode& node) -> bool
        {
            if (node.StyleID && !node.StyleID->IsValid())
            {
                outError = "Invalid UI style UUID on node: " + Core::UUIDToString(node.ID);
                return false;
            }

            for (const UIDocumentNode& child : node.Children)
            {
                if (!visit(visit, child)) return false;
            }

            return true;
        };

        for (const UIDocumentNode& root : document.Roots)
        {
            if (!validateStyleIDs(validateStyleIDs, root)) return false;
        }

        using json = nlohmann::json;
        const auto alignmentName = [](const Alignment alignment) -> const char*
        {
            switch (alignment)
            {
                case Alignment::Start:   return "start";
                case Alignment::Center:  return "center";
                case Alignment::End:     return "end";
                case Alignment::Stretch: return "stretch";
            }

            return "";
        };

        const auto hitTestName = [](const HitTestMode mode) -> const char*
        {
            switch (mode)
            {
                case HitTestMode::SelfAndChildren: return "self_and_children";
                case HitTestMode::ChildrenOnly:    return "children_only";
                case HitTestMode::None:            return "none";
            }
            return "";
        };

        const auto serializeNode = [&](const auto& visit, const UIDocumentNode& node) -> json
        {
            json result;

            result["id"] = Core::UUIDToString(node.ID);
            result["name"] = node.Name;
            result["bounds"] = {
                { "position", json::array({ node.Bounds.Position.x, node.Bounds.Position.y }) },
                { "size", json::array({ node.Bounds.Size.x, node.Bounds.Size.y }) }
            };
            result["anchor"] = json::array({ node.Anchor.x, node.Anchor.y });
            result["pivot"] = json::array({ node.Pivot.x, node.Pivot.y });

            result["horizontal_alignment"] = alignmentName(node.HorizontalAlignment);
            result["vertical_alignment"] = alignmentName(node.VerticalAlignment);
            result["enabled"] = node.Enabled;
            result["clip_to_bounds"] = node.ClipToBounds;

            result["focusable"] = nullptr;
            if (node.Focusable)
            {
                result["focusable"] = *node.Focusable;
            }

            result["hit_test"] = nullptr;
            if (node.HitTest)
            {
                result["hit_test"] = hitTestName(*node.HitTest);
            }

            result["style_id"] = nullptr;
            if (node.StyleID)
            {
                result["style_id"] = Core::UUIDToString(*node.StyleID);
            }

            json properties = json::object();

            if (std::holds_alternative<UIDocumentWidget>(node.Properties))
            {
                result["type"] = "widget";
            }
            else if (std::holds_alternative<UIDocumentPanel>(node.Properties))
            {
                result["type"] = "panel";
            }
            else if (const auto* label = std::get_if<UIDocumentLabel>(&node.Properties))
            {
                result["type"] = "label";
                properties["content"] = label->Content;
            }
            else if (const auto* image = std::get_if<UIDocumentImage>(&node.Properties))
            {
                result["type"] = "image";
                properties["texture_id"] = image->TextureID;
            }
            else if (std::holds_alternative<UIDocumentButton>(node.Properties))
            {
                result["type"] = "button";
            }
            else if (const auto* box = std::get_if<UIDocumentHorizontalBox>(&node.Properties))
            {
                result["type"] = "horizontal_box";
                properties["padding"] = json::array({
                    box->Padding.x, box->Padding.y, box->Padding.z, box->Padding.w
                });
                properties["spacing"] = box->Spacing;
            }
            else if (const auto* box = std::get_if<UIDocumentVerticalBox>(&node.Properties))
            {
                result["type"] = "vertical_box";
                properties["padding"] = json::array({ box->Padding.x, box->Padding.y, box->Padding.z, box->Padding.w });
                properties["spacing"] = box->Spacing;
            }

            result["properties"] = std::move(properties);
            result["children"] = json::array();

            for (const UIDocumentNode& child : node.Children)
            {
                result["children"].push_back(visit(visit, child));
            }

            return result;
        };

        try
        {
            json root;
            root["version"] = 1;
            root["roots"] = json::array();

            for (const UIDocumentNode& node : document.Roots)
            {
                root["roots"].push_back(serializeNode(serializeNode, node));
            }

            std::string serialized = root.dump(4);
            outJSON = std::move(serialized);
            return true;
        }
        catch (const json::exception& exception)
        {
            outError = std::string("Failed to serialize UI document: ") + exception.what();
            return false;
        }
    }

    bool DeserializeUIDocument(const std::string& jsonText, UIDocument& outDocument, std::string& outError)
    {
        using json = nlohmann::json;
        outError.clear();

        try
        {
            const json root = json::parse(jsonText);

            if (!root.is_object() ||
                !root.at("version").is_number_integer() ||
                root.at("version") != 1)
            {
                throw std::runtime_error("Unsupported UI document version.");
            }

            const json& roots = root.at("roots");
            if (!roots.is_array())
            {
                throw std::runtime_error("UI document roots must be an array.");
            }

            const auto readVec2 = [](const json& value) -> glm::vec2
            {
                if (!value.is_array() || value.size() != 2)
                {
                    throw std::runtime_error("Expected a two-component UI vector.");
                }

                return {value.at(0).get<float>(), value.at(1).get<float>()};
            };

            const auto readVec4 = [](const json& value) -> glm::vec4
            {
                if (!value.is_array() || value.size() != 4)
                {
                    throw std::runtime_error("Expected a four-component UI vector.");
                }

                return {
                    value.at(0).get<float>(),
                    value.at(1).get<float>(),
                    value.at(2).get<float>(),
                    value.at(3).get<float>()
                };
            };

            const auto readUUID = [](const json& value) -> Core::UUID
            {
                Core::UUID id{};
                const std::string text = value.get<std::string>();

                if (!Core::UUIDFromString(text, id) || !id.IsValid())
                {
                    throw std::runtime_error("Invalid UI UUID: " + text);
                }

                return id;
            };

            const auto readAlignment = [](const json& value) -> Alignment
            {
                const std::string text = value.get<std::string>();

                if (text == "start")   return Alignment::Start;
                if (text == "center")  return Alignment::Center;
                if (text == "end")     return Alignment::End;
                if (text == "stretch") return Alignment::Stretch;

                throw std::runtime_error("Unknown UI alignment: " + text);
            };

            const auto readHitTest = [](const json& value) -> HitTestMode
            {
                const std::string text = value.get<std::string>();

                if (text == "self_and_children") return HitTestMode::SelfAndChildren;
                if (text == "children_only")     return HitTestMode::ChildrenOnly;
                if (text == "none")              return HitTestMode::None;

                throw std::runtime_error("Unknown UI hit-test mode: " + text);
            };

            const auto readNode = [&](const auto& visit, const json& value)
                -> UIDocumentNode
            {
                if (!value.is_object())
                {
                    throw std::runtime_error("UI node must be an object.");
                }

                UIDocumentNode node;
                node.ID = readUUID(value.at("id"));
                node.Name = value.at("name").get<std::string>();

                const json& bounds = value.at("bounds");
                node.Bounds.Position = readVec2(bounds.at("position"));
                node.Bounds.Size = readVec2(bounds.at("size"));
                node.Anchor = readVec2(value.at("anchor"));
                node.Pivot = readVec2(value.at("pivot"));
                node.HorizontalAlignment = readAlignment(value.at("horizontal_alignment"));
                node.VerticalAlignment = readAlignment(value.at("vertical_alignment"));
                node.Enabled = value.at("enabled").get<bool>();
                node.ClipToBounds = value.at("clip_to_bounds").get<bool>();

                const json& focusable = value.at("focusable");
                if (!focusable.is_null())
                {
                    node.Focusable = focusable.get<bool>();
                }

                const json& hitTest = value.at("hit_test");
                if (!hitTest.is_null())
                {
                    node.HitTest = readHitTest(hitTest);
                }

                const json& styleID = value.at("style_id");
                if (!styleID.is_null())
                {
                    node.StyleID = readUUID(styleID);
                }

                const std::string type = value.at("type").get<std::string>();
                const json& properties = value.at("properties");

                if (!properties.is_object())
                {
                    throw std::runtime_error("UI node properties must be an object.");
                }

                if (type == "widget")
                {
                    node.Properties = UIDocumentWidget{};
                }
                else if (type == "panel")
                {
                    node.Properties = UIDocumentPanel{};
                }
                else if (type == "label")
                {
                    UIDocumentLabel label;
                    label.Content = properties.at("content").get<std::string>();
                    node.Properties = std::move(label);
                }
                else if (type == "image")
                {
                    UIDocumentImage image;
                    image.TextureID = properties.at("texture_id").get<std::string>();
                    node.Properties = std::move(image);
                }
                else if (type == "button")
                {
                    node.Properties = UIDocumentButton{};
                }
                else if (type == "horizontal_box")
                {
                    UIDocumentHorizontalBox box;
                    box.Padding = readVec4(properties.at("padding"));
                    box.Spacing = properties.at("spacing").get<float>();
                    node.Properties = box;
                }
                else if (type == "vertical_box")
                {
                    UIDocumentVerticalBox box;
                    box.Padding = readVec4(properties.at("padding"));
                    box.Spacing = properties.at("spacing").get<float>();
                    node.Properties = box;
                }
                else
                {
                    throw std::runtime_error("Unknown UI widget type: " + type);
                }

                const json& children = value.at("children");
                if (!children.is_array())
                {
                    throw std::runtime_error("UI node children must be an array.");
                }

                for (const json& child : children)
                {
                    node.Children.push_back(visit(visit, child));
                }

                return node;
            };

            UIDocument document;

            for (const json& node : roots)
            {
                document.Roots.push_back(readNode(readNode, node));
            }

            if (!ValidateUIDocumentNodeIDs(document, outError) ||
                !ValidateUIDocumentProperties(document, outError))
            {
                return false;
            }

            outDocument = std::move(document);
            return true;
        }
        catch (const json::exception& exception)
        {
            outError = std::string("Failed to read UI document JSON: ") + exception.what();
            return false;
        }
        catch (const std::runtime_error& exception)
        {
            outError = std::string("Invalid UI document: ") + exception.what();
            return false;
        }
    }

    bool SaveUIDocumentToFile(const std::filesystem::path& path, const UIDocument& document, std::string& outError)
    {
        std::string jsonText;
        if (!SerializeUIDocument(document, jsonText, outError))
        {
            return false;
        }

        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        if (!output.is_open())
        {
            outError = "Failed to open UI document for writing: " + path.string();
            return false;
        }

        output << jsonText;
        output.close();
        if (!output)
        {
            outError = "Failed to write UI document: " + path.string();
            return false;
        }

        return true;
    }

    bool LoadUIDocumentFromFile(const std::filesystem::path& path, UIDocument& outDocument, std::string& outError)
    {
        outError.clear();

        std::ifstream input(path, std::ios::binary);
        if (!input.is_open())
        {
            outError = "Failed to open UI document for reading: " + path.string();
            return false;
        }

        std::string jsonText;
        std::array<char, 4096> buffer{};

        while (input.read(buffer.data(), buffer.size()) || input.gcount() > 0)
        {
            jsonText.append(buffer.data(), static_cast<std::size_t>(input.gcount()));
        }

        if (input.bad() || (!input.eof() && input.fail()))
        {
            outError = "Failed to read UI document: " + path.string();
            return false;
        }

        return DeserializeUIDocument(jsonText, outDocument, outError);
    }

#pragma endregion

#pragma region Build

    std::unique_ptr<Widget> BuildUIDocument(const UIDocument& document, std::string& outError)
    {
        return BuildUIDocument(document, UIStyleSheet{}, outError);
    }

    std::unique_ptr<Widget> BuildUIDocument(const UIDocument& document, const UIStyleSheet& sheet, std::string& outError)
    {
        if (!ValidateUIDocumentNodeIDs(document, outError) ||
            !ValidateUIDocumentProperties(document, outError) ||
            !ValidateUIStyleSheet(sheet, outError))
        {
            return nullptr;
        }

        const auto validateStyles = [&](const auto& visit, const UIDocumentNode& node) -> bool
        {
            if (node.StyleID)
            {
                const auto found = std::find_if(
                    sheet.Styles.begin(),
                    sheet.Styles.end(),
                    [&](const UIStyleEntry& entry)
                    {
                        return entry.ID == *node.StyleID;
                    });

                if (!node.StyleID->IsValid() || found == sheet.Styles.end())
                {
                    outError = "Missing UI style for node: " + Core::UUIDToString(node.ID);
                    return false;
                }

                const bool buttonMatch =
                    std::holds_alternative<UIDocumentButton>(node.Properties) &&
                    std::holds_alternative<ButtonStyle>(found->Properties);

                const bool labelMatch =
                    std::holds_alternative<UIDocumentLabel>(node.Properties) &&
                    std::holds_alternative<TextStyle>(found->Properties);

                if (!buttonMatch && !labelMatch)
                {
                    outError = "UI style type does not match node: " + Core::UUIDToString(node.ID);
                    return false;
                }
            }

            for (const UIDocumentNode& child : node.Children)
            {
                if (!visit(visit, child)) return false;
            }

            return true;
        };

        for (const UIDocumentNode& node : document.Roots)
        {
            if (!validateStyles(validateStyles, node)) return nullptr;
        }

        auto root = std::make_unique<Widget>();

        const auto buildNode = [&sheet](const auto& build, const UIDocumentNode& node) -> std::unique_ptr<Widget>
        {
            std::unique_ptr<Widget> widget;

            if (std::holds_alternative<UIDocumentWidget>(node.Properties))
            {
                widget = std::make_unique<Widget>();
            }
            else if (std::holds_alternative<UIDocumentPanel>(node.Properties))
            {
                widget = std::make_unique<Panel>();
            }
            else if (const auto* properties = std::get_if<UIDocumentLabel>(&node.Properties))
            {
                auto label = std::make_unique<Label>();
                label->SetContent(properties->Content);
                widget = std::move(label);
            }
            else if (const auto* properties = std::get_if<UIDocumentImage>(&node.Properties))
            {
                auto image = std::make_unique<Image>();
                image->SetTextureID(properties->TextureID);
                widget = std::move(image);
            }
            else if (std::holds_alternative<UIDocumentButton>(node.Properties))
            {
                widget = std::make_unique<Button>();
            }
            else if (const auto* properties = std::get_if<UIDocumentHorizontalBox>(&node.Properties))
            {
                auto box = std::make_unique<HorizontalBox>();
                box->SetPadding(properties->Padding);
                box->SetSpacing(properties->Spacing);
                widget = std::move(box);
            }
            else if (const auto* properties = std::get_if<UIDocumentVerticalBox>(&node.Properties))
            {
                auto box = std::make_unique<VerticalBox>();
                box->SetPadding(properties->Padding);
                box->SetSpacing(properties->Spacing);
                widget = std::move(box);
            }

            if (node.StyleID)
            {
                const auto found = std::find_if(
                    sheet.Styles.begin(),
                    sheet.Styles.end(),
                    [&](const UIStyleEntry& entry)
                    {
                        return entry.ID == *node.StyleID;
                    });

                if (auto* button = dynamic_cast<Button*>(widget.get()))
                {
                    button->SetStyle(std::get<ButtonStyle>(found->Properties));
                }
                else if (auto* label = dynamic_cast<Label*>(widget.get()))
                {
                    label->SetStyle(std::get<TextStyle>(found->Properties));
                }
            }

            widget->SetID(node.ID);
            widget->SetBounds(node.Bounds);
            widget->SetAnchorAndPivot(node.Anchor, node.Pivot);
            widget->SetAlignment(node.HorizontalAlignment, node.VerticalAlignment);
            widget->SetClipToBounds(node.ClipToBounds);

            if (node.Focusable)
            {
                widget->SetFocusable(*node.Focusable);
            }
            if (node.HitTest)
            {
                widget->SetHitTestMode(*node.HitTest);
            }
            widget->SetEnabled(node.Enabled);

            for (const UIDocumentNode& child : node.Children)
            {
                widget->AddChild(build(build, child));
            }

            return widget;
        };

        for (const UIDocumentNode& node : document.Roots)
        {
            root->AddChild(buildNode(buildNode, node));
        }

        return root;
    }

#pragma endregion
}

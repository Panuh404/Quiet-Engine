#include "Quiet/UI/UIDocument.h"

#include "Quiet/UI/Layout/HorizontalBox.h"
#include "Quiet/UI/Layout/VerticalBox.h"
#include "Quiet/UI/Widgets/Button.h"
#include "Quiet/UI/Widgets/Image.h"
#include "Quiet/UI/Widgets/Label.h"
#include "Quiet/UI/Widgets/Panel.h"

#include <memory>
#include <utility>
#include <cmath>
#include <unordered_set>

namespace Quiet::UI
{
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

    std::unique_ptr<Widget> BuildUIDocument(const UIDocument& document, std::string& outError)
    {
        if (!ValidateUIDocumentNodeIDs(document, outError) ||
            !ValidateUIDocumentProperties(document, outError))
        {
            return nullptr;
        }

        auto root = std::make_unique<Widget>();

        const auto buildNode = [](const auto& build, const UIDocumentNode& node) -> std::unique_ptr<Widget>
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
}

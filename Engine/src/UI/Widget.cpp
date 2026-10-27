#include "Quiet/UI/Widget.h"

#include <cmath>
#include <algorithm>
#include <ranges>
#include <stdexcept>
#include <utility>

#include <glm/common.hpp>

namespace Quiet::UI
{
    Widget::~Widget() = default;

    Widget& Widget::GetChild(const std::size_t index)
    {
        return *m_Children.at(index);
    }

    const Widget& Widget::GetChild(const std::size_t index) const
    {
        return *m_Children.at(index);
    }

    bool Widget::SetBounds(const Rect &bounds)
    {
        if (!std::isfinite(bounds.Position.x) ||
            !std::isfinite(bounds.Position.y) ||
            !std::isfinite(bounds.Size.x) ||
            !std::isfinite(bounds.Size.y) ||
            bounds.Size.x < 0.0f ||
            bounds.Size.y < 0.0f)
        {
            return false;
        }

        if (m_LocalBounds.Position == bounds.Position &&
            m_LocalBounds.Size == bounds.Size)
        {
            return true;
        }

        m_LocalBounds = bounds;
        Invalidate(InvalidationReason::Layout);
        return true;
    }

    bool Widget::SetArrangedBounds(const Rect& bounds)
    {
        if (!std::isfinite(bounds.Position.x) ||
            !std::isfinite(bounds.Position.y) ||
            !std::isfinite(bounds.Size.x) ||
            !std::isfinite(bounds.Size.y) ||
            bounds.Size.x < 0.0f ||
            bounds.Size.y < 0.0f)
        {
            return false;
        }

        if (m_ArrangedBounds &&
            m_ArrangedBounds->Position == bounds.Position &&
            m_ArrangedBounds->Size == bounds.Size)
        {
            return true;
        }

        m_ArrangedBounds = bounds;
        Invalidate(InvalidationReason::Paint);
        Invalidate(InvalidationReason::Interaction);
        return true;
    }

    void Widget::ClearArrangedBounds() noexcept
    {
        if (!m_ArrangedBounds) return;

        m_ArrangedBounds.reset();
        Invalidate(InvalidationReason::Paint);
        Invalidate(InvalidationReason::Interaction);
    }

    bool Widget::SetAnchorAndPivot(const glm::vec2& anchor, const glm::vec2& pivot)
    {
        if (!std::isfinite(anchor.x) ||
            !std::isfinite(anchor.y) ||
            !std::isfinite(pivot.x) ||
            !std::isfinite(pivot.y))
        {
            return false;
        }

        const glm::vec2 clampedAnchor = glm::clamp(anchor, glm::vec2(0.0f), glm::vec2(1.0f));
        const glm::vec2 clampedPivot = glm::clamp(pivot, glm::vec2(0.0f), glm::vec2(1.0f));

        if (m_Anchor == clampedAnchor && m_Pivot == clampedPivot)
        {
            return true;
        }

        m_Anchor = clampedAnchor;
        m_Pivot = clampedPivot;
        Invalidate(InvalidationReason::Layout);
        return true;
    }

    void Widget::SetAlignment(Alignment horizontal, Alignment vertical) noexcept
    {
        if (m_HorizontalAlignment == horizontal && m_VerticalAlignment == vertical) return;
        m_HorizontalAlignment = horizontal;
        m_VerticalAlignment = vertical;
        Invalidate(InvalidationReason::Layout);
    }

    Rect Widget::GetSurfaceBounds() const noexcept
    {
        Rect bounds = m_ArrangedBounds.value_or(m_LocalBounds);
        if (m_Parent)
        {
            const Rect parentBounds = m_Parent->GetSurfaceBounds();
            bounds.Position += parentBounds.Position;

            if (!m_ArrangedBounds)
            {
                bounds.Position += m_Anchor * parentBounds.Size - m_Pivot * bounds.Size;
            }
        }
        return bounds;
    }

    void Widget::SetClipToBounds(const bool enabled) noexcept
    {
        if (m_ClipToBounds == enabled) return;
        m_ClipToBounds = enabled;

        Invalidate(InvalidationReason::Paint);
        Invalidate(InvalidationReason::Interaction);
    }

    std::optional<Rect> Widget::GetSurfaceClip() const noexcept
    {
        std::optional<Rect> clip;

        for (const Widget* widget = this; widget != nullptr; widget = widget->m_Parent)
        {
            if (!widget->m_ClipToBounds) continue;
            const Rect bounds = widget->GetSurfaceBounds();

            if (!clip)
            {
                clip = bounds;
            }
            else
            {
                const glm::vec2 minimum = glm::max(clip->Position, bounds.Position);
                const glm::vec2 maximum = glm::min(clip->Position + clip->Size, bounds.Position + bounds.Size);

                clip = Rect
                {
                    minimum,
                    glm::max(maximum - minimum, glm::vec2(0.0f))
                };
            }

            if (clip->Size.x <= 0.0f || clip->Size.y <= 0.0f)
            {
                break;
            }
        }

        return clip;
    }

    void Widget::SetHitTestMode(const HitTestMode mode) noexcept
    {
        if (m_HitTestMode == mode) return;

        m_HitTestMode = mode;
        Invalidate(InvalidationReason::Interaction);
    }

    Widget* Widget::HitTest(const glm::vec2& surfacePosition) noexcept
    {
        if (!std::isfinite(surfacePosition.x) || !std::isfinite(surfacePosition.y) || m_HitTestMode == HitTestMode::None)
        {
            return nullptr;
        }

        if (const auto clip = GetSurfaceClip())
        {
            const glm::vec2 maximum = clip->Position + clip->Size;

            if (clip->Size.x <= 0.0f ||
                clip->Size.y <= 0.0f ||
                surfacePosition.x < clip->Position.x ||
                surfacePosition.y < clip->Position.y ||
                surfacePosition.x >= maximum.x ||
                surfacePosition.y >= maximum.y)
            {
                return nullptr;
            }
        }

        for (auto & child : std::views::reverse(m_Children))
        {
            if (Widget* hit = child->HitTest(surfacePosition)) return hit;
        }

        if (m_HitTestMode != HitTestMode::SelfAndChildren) return nullptr;

        const Rect bounds = GetSurfaceBounds();
        if (bounds.Size.x <= 0.0f || bounds.Size.y <= 0.0f) return nullptr;

        const glm::vec2 maximum = bounds.Position + bounds.Size;
        if (surfacePosition.x >= bounds.Position.x &&
            surfacePosition.y >= bounds.Position.y &&
            surfacePosition.x < maximum.x &&
            surfacePosition.y < maximum.y)
        {
            return this;
        }

        return nullptr;
    }

    void Widget::UpdatePointerHover(const Widget* hoveredWidget) noexcept
    {
        const bool hovered = hoveredWidget == this;
        if (m_PointerHovered != hovered)
        {
            m_PointerHovered = hovered;
            Invalidate(InvalidationReason::Paint);
        }

        for (const auto& child : m_Children)
        {
            child->UpdatePointerHover(hoveredWidget);
        }
    }

    Widget* Widget::UpdatePointerPress(const Widget* hoveredWidget, bool primaryPressed, bool primaryDown, bool primaryReleased) noexcept
    {
        if (m_HitTestMode == HitTestMode::None)
        {
            primaryPressed = false;
            primaryDown = false;
            primaryReleased = false;
        }

        const bool acceptsPress = m_HitTestMode == HitTestMode::SelfAndChildren;
        const bool ownsPress = primaryPressed ? hoveredWidget == this : m_PointerPressed;

        Widget* activatedWidget =
            acceptsPress &&
            ownsPress &&
            primaryReleased &&
            hoveredWidget == this
                ? this
                : nullptr;

        const bool pressed =
            acceptsPress &&
            ownsPress &&
            primaryDown &&
            !primaryReleased;

        if (m_PointerPressed != pressed)
        {
            m_PointerPressed = pressed;
            Invalidate(InvalidationReason::Paint);
        }

        for (const auto& child : m_Children)
        {
            if (Widget* activatedChild = child->UpdatePointerPress(hoveredWidget, primaryPressed, primaryDown, primaryReleased))
            {
                activatedWidget = activatedChild;
            }
        }

        return activatedWidget;
    }

    void Widget::Activate()
    {
    }

    Widget& Widget::AddChild(std::unique_ptr<Widget>&& child)
    {
        if (!child)
        {
            throw std::invalid_argument("Cannot attach a null UI widget.");
        }

        for (const Widget* ancestor = this; ancestor != nullptr; ancestor = ancestor->m_Parent)
        {
            if (ancestor == child.get())
            {
                throw std::invalid_argument("Cannot create a cycle in the UI widget hierarchy.");
            }
        }

        Widget& attachedChild = *child;

        m_Children.push_back(std::move(child));
        attachedChild.m_Parent = this;
        attachedChild.ClearArrangedBounds();
        attachedChild.UpdatePointerHover(nullptr);
        attachedChild.UpdatePointerPress(nullptr, false, false);
        attachedChild.Invalidate(InvalidationReason::Layout);
        return attachedChild;
    }

    std::unique_ptr<Widget> Widget::DetachChild(const Widget& child)
    {
        const auto iterator = std::find_if(
            m_Children.begin(),
            m_Children.end(),
            [&child](const std::unique_ptr<Widget>& candidate)
            {
                return candidate.get() == &child;
            });

        if (iterator == m_Children.end())
        {
            return nullptr;
        }

        std::unique_ptr<Widget> detachedChild = std::move(*iterator);
        m_Children.erase(iterator);
        detachedChild->m_Parent = nullptr;
        detachedChild->ClearArrangedBounds();
        detachedChild->UpdatePointerHover(nullptr);
        detachedChild->UpdatePointerPress(nullptr, false, false);
        detachedChild->Invalidate(InvalidationReason::Layout);
        Invalidate(InvalidationReason::Layout);
        return detachedChild;
    }

    void Widget::Invalidate(InvalidationReason reason) noexcept
    {
        for (Widget* widget = this; widget != nullptr; widget = widget->m_Parent)
        {
            switch (reason)
            {
                case InvalidationReason::Layout:
                    widget->m_Invalidation.Layout = true;
                    widget->m_Invalidation.Paint = true;
                    widget->m_Invalidation.Interaction = true;
                    break;

                case InvalidationReason::Paint:
                    widget->m_Invalidation.Paint = true;
                    break;

                case InvalidationReason::Interaction:
                    widget->m_Invalidation.Interaction = true;
                    break;
            }
        }
    }

    void Widget::ClearInvalidation(InvalidationReason reason) noexcept
    {
        switch (reason)
        {
            case InvalidationReason::Layout:
                m_Invalidation.Layout = false;
                break;

            case InvalidationReason::Paint:
                m_Invalidation.Paint = false;
                break;

            case InvalidationReason::Interaction:
                m_Invalidation.Interaction = false;
                break;
        }

        for (const auto& child : m_Children)
        {
            child->ClearInvalidation(reason);
        }
    }

    void Widget::Measure(const LayoutContext& context)
    {
        for (const auto& child : m_Children)
        {
            child->Measure(context);
        }

        const glm::vec2 desiredSize = OnMeasure(context);

        if (!std::isfinite(desiredSize.x) ||
            !std::isfinite(desiredSize.y) ||
            desiredSize.x < 0.0f ||
            desiredSize.y < 0.0f)
        {
            throw std::runtime_error("UI widget measurement produced invalid dimensions.");
        }

        m_DesiredSize = desiredSize;
    }

    void Widget::ArrangeChildren()
    {
        OnArrangeChildren();
        for (const auto& child : m_Children)
        {
            child->ArrangeChildren();
        }
    }

    void Widget::Paint(PaintList& commands) const
    {
        const std::optional<Rect> clip = GetSurfaceClip();

        if (clip && (clip->Size.x <= 0.0f || clip->Size.y <= 0.0f)) return;

        const std::size_t firstCommand = commands.size();
        OnPaint(commands);

        for (std::size_t index = firstCommand; index < commands.size(); ++index)
        {
            std::visit([&clip](auto& command)
            {
                command.Clip = clip;
            },
            commands[index]);
        }

        for (const auto& child : m_Children)
        {
            child->Paint(commands);
        }
    }
    glm::vec2 Widget::OnMeasure(const LayoutContext&) const
    {
        return m_LocalBounds.Size;
    }

    void Widget::OnArrangeChildren()
    {
        const glm::vec2 parentSize = GetSurfaceBounds().Size;

        for (const auto& child : m_Children)
        {
            const Rect& authoredBounds = child->GetLocalBounds();
            const glm::vec2& desiredSize = child->GetDesiredSize();

            const glm::vec2 position =
                authoredBounds.Position +
                child->GetAnchor() * parentSize -
                child->GetPivot() * desiredSize;

            child->SetArrangedBounds(
            {
                position,
                desiredSize
            });
        }
    }

    void Widget::OnPaint(PaintList &commands) const {}
}

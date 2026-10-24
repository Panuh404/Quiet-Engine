#include "Quiet/UI/Widget.h"

#include <cmath>
#include <algorithm>
#include <ranges>
#include <stdexcept>
#include <utility>

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

    Rect Widget::GetSurfaceBounds() const noexcept
    {
        Rect bounds = m_LocalBounds;
        for (const Widget* ancestor = m_Parent; ancestor != nullptr; ancestor = ancestor->m_Parent)
        {
            bounds.Position += ancestor->m_LocalBounds.Position;
        }
        return bounds;
    }

    void Widget::SetHitTestMode(const HitTestMode mode) noexcept
    {
        if (m_HitTestMode == mode) return;

        m_HitTestMode = mode;
        Invalidate(InvalidationReason::Interaction);
    }

    Widget* Widget::HitTest(const glm::vec2& surfacePosition) noexcept
    {
        if (!std::isfinite(surfacePosition.x) ||
            !std::isfinite(surfacePosition.y) ||
            m_HitTestMode == HitTestMode::None)
        {
            return nullptr;
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

    void Widget::UpdatePointerPress(const Widget* hoveredWidget, bool primaryPressed, bool primaryDown) noexcept
    {
        if (m_HitTestMode == HitTestMode::None)
        {
            primaryPressed = false;
            primaryDown = false;
        }

        const bool pressed =
            m_HitTestMode == HitTestMode::SelfAndChildren &&
            primaryDown &&
            (primaryPressed ? hoveredWidget == this : m_PointerPressed);

        if (m_PointerPressed != pressed)
        {
            m_PointerPressed = pressed;
            Invalidate(InvalidationReason::Paint);
        }

        for (const auto& child : m_Children)
        {
            child->UpdatePointerPress(hoveredWidget, primaryPressed, primaryDown);
        }
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

    void Widget::Paint(PaintList &commands) const
    {
        OnPaint(commands);
        for (const auto& child : m_Children)
        {
            child->Paint(commands);
        }
    }

    void Widget::OnPaint(PaintList &commands) const
    {
    }
}

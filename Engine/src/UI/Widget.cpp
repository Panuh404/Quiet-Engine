#include "Quiet/UI/Widget.h"

#include <cmath>
#include <algorithm>
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

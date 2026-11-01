#include "Quiet/UI/Widget.h"

#include <algorithm>
#include <cmath>
#include <ranges>
#include <vector>

#include <glm/common.hpp>

namespace Quiet::UI
{
#pragma region Focus and enabled state

    void Widget::SetFocusable(const bool focusable) noexcept
    {
        if (m_Focusable == focusable) return;
        m_Focusable = focusable;

        if (!focusable && (m_Focused || m_FocusedPressed))
        {
            m_Focused = false;
            m_FocusedPressed = false;
            Invalidate(InvalidationReason::Paint);
        }

        Invalidate(InvalidationReason::Interaction);
    }

    bool Widget::CanReceiveFocus() const noexcept
    {
        if (!m_Focusable || !IsEffectivelyEnabled()) return false;

        const Rect bounds = GetSurfaceBounds();
        if (bounds.Size.x <= 0.0f || bounds.Size.y <= 0.0f) return false;

        const Widget* root = this;
        while (root->m_Parent)
        {
            root = root->m_Parent;
        }

        const Rect rootBounds = root->GetSurfaceBounds();

        glm::vec2 minimum = glm::max(bounds.Position, rootBounds.Position);
        glm::vec2 maximum = glm::min(bounds.Position + bounds.Size, rootBounds.Position + rootBounds.Size);

        if (const auto clip = GetSurfaceClip())
        {
            minimum = glm::max(minimum, clip->Position);
            maximum = glm::min(maximum, clip->Position + clip->Size);
        }

        return maximum.x > minimum.x && maximum.y > minimum.y;
    }

    bool Widget::SetFocus(Widget* target) noexcept
    {
        if (m_Parent) return false;
        if (target)
        {
            const Widget* targetRoot = target;
            while (targetRoot->m_Parent)
            {
                targetRoot = targetRoot->m_Parent;
            }
            if (targetRoot != this || !target->CanReceiveFocus())
            {
                return false;
            }
        }

        if (GetFocusedWidget() == target) return true;
        ClearFocus();

        if (target)
        {
            target->m_Focused = true;
            target->Invalidate(InvalidationReason::Paint);
        }

        return true;
    }

    void Widget::ClearFocus() noexcept
    {
        if (m_Focused || m_FocusedPressed)
        {
            m_Focused = false;
            m_FocusedPressed = false;
            Invalidate(InvalidationReason::Paint);
        }

        for (const auto& child : m_Children)
        {
            child->ClearFocus();
        }
    }

    Widget* Widget::GetFocusedWidget() noexcept
    {
        if (m_Focused)
        {
            if (CanReceiveFocus()) return this;
            m_Focused = false;
            m_FocusedPressed = false;
            Invalidate(InvalidationReason::Paint);
        }

        for (const auto& child : m_Children)
        {
            if (Widget* focused = child->GetFocusedWidget()) return focused;
        }

        return nullptr;
    }

    Widget* Widget::MoveFocus(const bool reverse)
    {
        if (m_Parent) return nullptr;

        std::vector<Widget*> candidates;

        const auto collect = [&candidates](const auto& visit, Widget& widget) -> void
        {
            if (!widget.IsEffectivelyEnabled()) return;
            if (widget.CanReceiveFocus()) candidates.push_back(&widget);

            for (const auto& child : widget.m_Children)
            {
                visit(visit, *child);
            }
        };

        collect(collect, *this);
        if (candidates.empty())
        {
            ClearFocus();
            return nullptr;
        }

        Widget* current = GetFocusedWidget();
        const auto found = std::find(candidates.begin(), candidates.end(), current);
        std::size_t index = reverse ? candidates.size() - 1 : 0;

        if (found != candidates.end())
        {
            const auto currentIndex = static_cast<std::size_t>(found - candidates.begin());
            index = reverse
                ? (currentIndex == 0 ? candidates.size() - 1 : currentIndex - 1)
                : (currentIndex + 1) % candidates.size();
        }

        Widget* selected = candidates[index];
        SetFocus(selected);
        return selected;
    }

    void Widget::SetEnabled(const bool enabled) noexcept
    {
        if (m_Enabled == enabled) return;
        m_Enabled = enabled;

        if (!enabled)
        {
            ClearFocus();
            UpdatePointerHover(nullptr);
            UpdatePointerPress(nullptr, false, false);
        }

        Invalidate(InvalidationReason::Paint);
        Invalidate(InvalidationReason::Interaction);
    }

    bool Widget::IsEffectivelyEnabled() const noexcept
    {
        for (const Widget* widget = this; widget != nullptr; widget = widget->m_Parent)
        {
            if (!widget->m_Enabled) return false;
        }

        return true;
    }

#pragma endregion

#pragma region Pointer input and activation

    void Widget::SetHitTestMode(const HitTestMode mode) noexcept
    {
        if (m_HitTestMode == mode) return;

        m_HitTestMode = mode;
        Invalidate(InvalidationReason::Interaction);
    }

    Widget* Widget::HitTest(const glm::vec2& surfacePosition) noexcept
    {
        if (!std::isfinite(surfacePosition.x) || !std::isfinite(surfacePosition.y) || m_HitTestMode == HitTestMode::None) return nullptr;
        if (const auto clip = GetSurfaceClip())
        {
            const glm::vec2 maximum = clip->Position + clip->Size;

            if (clip->Size.x <= 0.0f ||
                clip->Size.y <= 0.0f ||
                surfacePosition.x < clip->Position.x ||
                surfacePosition.y < clip->Position.y ||
                surfacePosition.x >= maximum.x ||
                surfacePosition.y >= maximum.y)
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
            return this;

        return nullptr;
    }

    void Widget::UpdatePointerHover(const Widget* hoveredWidget) noexcept
    {
        const bool hovered = hoveredWidget == this && IsEffectivelyEnabled();
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
        const bool enabled = IsEffectivelyEnabled();
        if (!enabled || m_HitTestMode == HitTestMode::None)
        {
            primaryPressed = false;
            primaryDown = false;
            primaryReleased = false;
        }

        const bool acceptsPress = enabled && m_HitTestMode == HitTestMode::SelfAndChildren;
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

    Widget* Widget::UpdateFocusedPress(const bool pressed, const bool down, const bool released) noexcept
    {
        const bool acceptsPress = m_Focused && CanReceiveFocus();
        const bool ownsPress = pressed ? acceptsPress : m_FocusedPressed;

        Widget* activated = acceptsPress && ownsPress && released ? this : nullptr;

        const bool focusedPressed = acceptsPress && ownsPress && down && !released;
        if (m_FocusedPressed != focusedPressed)
        {
            m_FocusedPressed = focusedPressed;
            Invalidate(InvalidationReason::Paint);
        }

        for (const auto& child : m_Children)
        {
            if (Widget* candidate = child->UpdateFocusedPress(pressed, down, released))
            {
                activated = candidate;
            }
        }

        return activated;
    }

    void Widget::Activate() {}

#pragma endregion
}
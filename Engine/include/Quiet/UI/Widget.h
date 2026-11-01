#pragma once

#include <optional>
#include <cstddef>
#include <memory>
#include <vector>

#include "Geometry.h"
#include "Layout/LayoutContext.h"
#include "Quiet/UI/PaintCommands.h"

namespace Quiet::UI
{
    /**
     * @brief Positions a child within space allocated by a container.
     * @ingroup quiet_renderer
     */
    enum class Alignment
    {
        Start,
        Center,
        End,
        Stretch
    };

    /**
     * @brief Controls participation in pointer hit testing.
     * @ingroup quiet_renderer
     */
    enum class HitTestMode
    {
        SelfAndChildren,
        ChildrenOnly,
        None
    };

    /**
     * @brief Identifies work invalidated by a widget mutation.
     * @ingroup quiet_renderer
     */
    enum class InvalidationReason
    {
        Layout,
        Paint,
        Interaction
    };

    /**
     * @brief Records pending work for a widget or its subtree.
     * @ingroup quiet_renderer
     */
    struct InvalidationState
    {
        bool Layout = true;
        bool Paint = true;
        bool Interaction = true;
    };

    /**
     * @brief Owns children in a retained runtime UI hierarchy.
     * @ingroup quiet_renderer
     *
     * Parents exclusively own their children. Parent pointers and references
     * returned by this API are borrowed and do not extend widget lifetime.
     *
     * Geometry, pointer queries, and OnPaint() output use logical UI pixels.
     * Application converts painting output to rendered target pixels.
     *
     * Widgets have stable addresses while owned, including when the parent's
     * child vector grows. Destruction recursively destroys owned children.
     */
    class Widget
    {
    public:
        Widget() = default;
        virtual ~Widget();

        Widget(const Widget&) = delete;
        Widget& operator=(const Widget&) = delete;
        Widget(Widget&&) = delete;
        Widget& operator=(Widget&&) = delete;

        #pragma region Hierarchy

        [[nodiscard]] Widget* GetParent() noexcept { return m_Parent; }
        [[nodiscard]] const Widget* GetParent() const noexcept { return m_Parent; }
        [[nodiscard]] std::size_t GetChildCount() const noexcept { return m_Children.size(); }

        /**
         * @brief Returns a borrowed direct child in insertion order.
         * @throws std::out_of_range If the index is outside the child collection.
         */
        [[nodiscard]] Widget& GetChild(std::size_t index);
        [[nodiscard]] const Widget& GetChild(std::size_t index) const;

        /**
         * @brief Transfers exclusive ownership of a detached subtree.
         * @return Borrowed reference to the attached child.
         * @throws std::invalid_argument If the child is null or creates a cycle.
         * @note Validation occurs before moving the supplied owner. Allocation
         *       exceptions propagate. Never create an owner from a borrowed pointer.
         */
        Widget& AddChild(std::unique_ptr<Widget>&& child);

        /**
         * @brief Transfers ownership of a direct child to the caller.
         * @return Detached subtree, or null if the widget is not a direct child.
         * @note Descendants remain attached. Destroying the returned owner destroys
         *       the subtree and invalidates borrowed references to its widgets.
         */
        [[nodiscard]] std::unique_ptr<Widget> DetachChild(const Widget& child);

        #pragma endregion

        #pragma region Geometry and layout

        /**
         * @brief Sets the authored pixel offset and explicit size request.
         * @return False for nonfinite values or negative dimensions; otherwise true.
         * @note Invalid values leave existing bounds unchanged. Existing arranged
         *       bounds are not overwritten immediately.
         */
        bool SetBounds(const Rect& bounds);
        [[nodiscard]] const Rect& GetLocalBounds() const noexcept { return m_LocalBounds; }

        /**
         * @brief Assigns a parent-relative rectangle during arrangement.
         * @return False for nonfinite values or negative dimensions.
         * @note Preserves authored bounds, anchor, and pivot. The assigned rectangle
         *       bypasses anchor/pivot resolution and invalidates paint and interaction
         *       without requesting another layout.
         */
        bool SetArrangedBounds(const Rect& bounds);

        /**
         * @brief Removes the container-assigned rectangle.
         * @note Restores authored anchor/offset/pivot resolution.
         * @pre Call during arrangement or hierarchy ownership changes.
         */
        void ClearArrangedBounds() noexcept;

        /**
         * @brief Sets the normalized parent anchor and widget pivot.
         * @return False for nonfinite input, preserving existing placement.
         * @note Finite values are clamped to [0, 1]. Bounds position is the pixel
         *       offset from the anchor. Parentless widgets ignore anchor and pivot;
         *       container-assigned bounds take precedence.
         */
        bool SetAnchorAndPivot(const glm::vec2& anchor, const glm::vec2& pivot);
        [[nodiscard]] const glm::vec2& GetAnchor() const noexcept { return m_Anchor; }
        [[nodiscard]] const glm::vec2& GetPivot() const noexcept { return m_Pivot; }

        /**
         * @brief Sets alignment within container-allocated space.
         * @note VerticalBox uses horizontal alignment, HorizontalBox uses vertical
         *       alignment, and Button uses both. This does not change desired size.
         */
        void SetAlignment(Alignment horizontal, Alignment vertical) noexcept;
        [[nodiscard]] Alignment GetHorizontalAlignment() const noexcept { return m_HorizontalAlignment; }
        [[nodiscard]] Alignment GetVerticalAlignment() const noexcept { return m_VerticalAlignment; }

        /**
         * @brief Resolves authored or arranged geometry into logical surface bounds.
         * @note Arranged bounds bypass anchor/pivot resolution. Resolution is on
         *       demand and does not apply scaling or rotation.
         * @pre Intermediate and final coordinates must remain finite.
         */
        [[nodiscard]] Rect GetSurfaceBounds() const noexcept;

        /**
         * @brief Measures descendants, then this widget's preferred size.
         * @param context CPU resources borrowed only during this call.
         * @note Stores an unconstrained size without assigning geometry or clearing
         *       flags. Allocation and measurement exceptions propagate.
         * @pre The hierarchy and authored layout inputs must remain unchanged.
         */
        void Measure(const LayoutContext& context = {});

        /**
         * @brief Returns the size from the last successful measurement.
         * @pre Measure() must have completed after layout inputs changed.
         */
        [[nodiscard]] const glm::vec2& GetDesiredSize() const noexcept { return m_DesiredSize; }

        /**
         * @brief Arranges direct children and then their descendants.
         * @note Does not change this widget's bounds or clear invalidation flags.
         * @pre The hierarchy must remain unchanged, and measurement must be current.
         */
        void ArrangeChildren();

        #pragma endregion

        #pragma region Clipping and painting

        /**
         * @brief Clips this widget and its descendants to this widget's bounds.
         * @note Does not change layout. Enabled ancestor clips still apply.
         */
        void SetClipToBounds(bool enabled) noexcept;
        [[nodiscard]] bool GetClipToBounds() const noexcept { return m_ClipToBounds; }

        /**
         * @brief Resolves enabled clips on this widget's ancestor chain.
         * @return No value if clipping is absent; otherwise the effective surface
         *         rectangle, which may be empty.
         * @pre Resolved bounds and their maximum coordinates must remain finite.
         */
        [[nodiscard]] std::optional<Rect> GetSurfaceClip() const noexcept;

        /**
         * @brief Appends this subtree's painting commands to a caller-owned list.
         * @note Paints this widget first, then children in insertion order. Commands
         *       use logical UI coordinates; Application applies scaling.
         *       Paint() applies the effective clip, so OnPaint() leaves Clip unset.
         * @pre The hierarchy and paint-affecting state must remain unchanged.
         */
        void Paint(PaintList& commands) const;

        #pragma endregion

        #pragma region Focus and enabled state

        /**
         * @brief Sets whether this widget can receive focus.
         * @note Defaults to false. Disabling focusability clears this widget's focus.
         */
        void SetFocusable(bool focusable) noexcept;
        [[nodiscard]] bool IsFocusable() const noexcept { return m_Focusable; }

        /**
         * @brief Tests focus eligibility using current arranged geometry.
         * @note Requires focusability, effective enabled state, and positive visible
         *       area within root bounds and ancestor clips. Does not test sibling
         *       occlusion. HitTestMode controls pointer input independently.
         * @pre Layout must be current and resolved coordinates finite.
         */
        [[nodiscard]] bool CanReceiveFocus() const noexcept;
        [[nodiscard]] bool IsFocused() const noexcept { return m_Focused && CanReceiveFocus(); }

        /**
         * @brief Selects an eligible widget in this root, or clears focus.
         * @return False for a non-root receiver, foreign target, or ineligible target.
         * @note Invalid requests preserve existing valid focus; null clears it.
         * @pre target must be null or a live borrowed widget pointer.
         */
        bool SetFocus(Widget* target) noexcept;
        void ClearFocus() noexcept;

        /**
         * @brief Returns the focused descendant and clears obsolete focus.
         * @return Borrowed pointer, or null.
         * @pre Layout must be current.
         */
        [[nodiscard]] Widget* GetFocusedWidget() noexcept;

        /**
         * @brief Moves focus through eligible widgets in hierarchy order.
         * @return Borrowed selected widget, or null if none are eligible.
         * @note Traversal is depth-first in insertion order, or its reverse, and
         *       wraps. With no focus, selects the first or last candidate.
         *       Allocation exceptions propagate before selection changes.
         * @pre Call on the root with current layout and an unchanged hierarchy.
         */
        Widget* MoveFocus(bool reverse = false);

        /**
         * @brief Enables or disables interaction for this subtree.
         * @note Disabling cancels hover and pending presses while preserving layout,
         *       visibility, and hit-test geometry. Re-enabling restores no press.
         */
        void SetEnabled(bool enabled) noexcept;
        [[nodiscard]] bool IsEnabled() const noexcept { return m_Enabled; }

        /**
         * @brief Returns whether this widget and all its ancestors are enabled.
         */
        [[nodiscard]] bool IsEffectivelyEnabled() const noexcept;

        #pragma endregion

        #pragma region Pointer input and activation

        /**
         * @brief Sets pointer hit-test participation without changing painting.
         * @note ChildrenOnly searches descendants but skips this widget; None skips
         *       the entire subtree.
         */
        void SetHitTestMode(HitTestMode mode) noexcept;
        [[nodiscard]] HitTestMode GetHitTestMode() const noexcept { return m_HitTestMode; }

        /**
         * @brief Finds the topmost participating widget at a surface-space point.
         * @return Borrowed widget pointer, or null.
         * @note Searches children in reverse painting order, applies ancestor clips,
         *       includes minimum edges, and excludes maximum edges. Nonfinite points
         *       produce no hit. Disabled widgets retain hit-test participation.
         * @pre The hierarchy must remain unchanged and resolved coordinates finite.
         */
        [[nodiscard]] Widget* HitTest(const glm::vec2& surfacePosition) noexcept;

        /**
         * @brief Updates direct hover state throughout this subtree.
         * @param hoveredWidget A hit-test result from this root, or null.
         * @note Stores no pointer; only the directly hit widget is hovered.
         *       Paint is invalidated only when state changes.
         * @pre The hierarchy must remain unchanged during traversal.
         */
        void UpdatePointerHover(const Widget* hoveredWidget) noexcept;
        [[nodiscard]] bool IsPointerHovered() const noexcept { return m_PointerHovered; }

        /**
         * @brief Updates primary-button ownership and identifies release activation.
         * @param hoveredWidget Current hit-test result, or null.
         * @param primaryPressed True on the primary-button down transition.
         * @param primaryDown True while the primary button remains held.
         * @param primaryReleased True on the primary-button up transition.
         * @return Borrowed activation target, or null.
         * @note Press starts on the directly hit widget and activates on release
         *       only if it remains the hit target. All-false input cancels.
         *       None cancels and excludes the subtree. Does not invoke callbacks.
         * @pre Call on the input root without modifying the hierarchy.
         */
        Widget* UpdatePointerPress(const Widget* hoveredWidget, bool primaryPressed, bool primaryDown, bool primaryReleased = false) noexcept;
        [[nodiscard]] bool IsPointerPressed() const noexcept { return m_PointerPressed; }

        /**
         * @brief Updates a focused-widget activation gesture.
         * @return Borrowed release-activation target, or null.
         * @note Press starts on the focused eligible widget. All-false input and
         *       focus changes cancel ownership. Does not invoke callbacks.
         * @pre Call on the input root after layout and focus are current, without
         *      modifying the hierarchy during traversal.
         */
        Widget* UpdateFocusedPress(bool pressed, bool down, bool released = false) noexcept;
        [[nodiscard]] bool IsFocusedPressed() const noexcept { return m_FocusedPressed; }

        /**
         * @brief Performs this widget's activation behavior.
         * @note The base implementation does nothing. Call after input traversal;
         *       exceptions propagate.
         */
        virtual void Activate();

        #pragma endregion

        #pragma region Invalidation

        /**
         * @brief Marks pending work on this widget and its ancestors.
         * @note Layout also invalidates paint and interaction.
         */
        void Invalidate(InvalidationReason reason) noexcept;
        [[nodiscard]] const InvalidationState& GetInvalidationState() const noexcept { return m_Invalidation; }

        /**
         * @brief Acknowledges one category on this widget and its descendants.
         * @note Call only after processing that category. Does not clear other
         *       categories or update ancestor flags.
         */
        void ClearInvalidation(InvalidationReason reason) noexcept;

        #pragma endregion

    protected:
        /**
         * @brief Calculates this widget's unconstrained preferred size.
         * @return Finite, nonnegative dimensions.
         * @note Descendants are already measured. The base implementation returns
         *       the authored explicit size.
         */
        virtual glm::vec2 OnMeasure(const LayoutContext& context) const;

        /**
         * @brief Assigns geometry to direct children.
         * @note The base implementation uses measured sizes and authored placement.
         *       Overrides must not change the hierarchy or ancestor geometry.
         */
        virtual void OnArrangeChildren();

        /**
         * @brief Appends this widget's own visual commands.
         * @note The base implementation draws nothing; Paint() traverses children.
         */
        virtual void OnPaint(PaintList& commands) const;

    private:
        Widget* m_Parent = nullptr;
        Rect m_LocalBounds{};
        std::optional<Rect> m_ArrangedBounds;
        glm::vec2 m_DesiredSize{0.0f};
        glm::vec2 m_Anchor{0.0f};
        glm::vec2 m_Pivot{0.0f};
        Alignment m_HorizontalAlignment = Alignment::Stretch;
        Alignment m_VerticalAlignment = Alignment::Stretch;

        bool m_Focusable = false;
        bool m_Focused = false;
        bool m_Enabled = true;
        bool m_ClipToBounds = false;
        HitTestMode m_HitTestMode = HitTestMode::ChildrenOnly;
        bool m_PointerHovered = false;
        bool m_PointerPressed = false;
        bool m_FocusedPressed = false;
        InvalidationState m_Invalidation{};
        std::vector<std::unique_ptr<Widget>> m_Children;
    };
}

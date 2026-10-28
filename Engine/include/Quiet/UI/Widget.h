#pragma once

#include <optional>
#include <cstddef>
#include <memory>
#include <vector>

#include "Geometry.h"
#include "Quiet/UI/LayoutContext.h"
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

        [[nodiscard]] Widget* GetParent() noexcept { return m_Parent; }
        [[nodiscard]] const Widget* GetParent() const noexcept { return m_Parent; }
        [[nodiscard]] std::size_t GetChildCount() const noexcept { return m_Children.size(); }

        /**
         * @brief Returns a borrowed direct child by insertion order.
         * @throws std::out_of_range When index is outside the child collection.
         */
        [[nodiscard]] Widget& GetChild(std::size_t index);
        [[nodiscard]] const Widget& GetChild(std::size_t index) const;

        /**
         * @brief Sets this widget's authored pixel offset and explicit size request.
         * @return False for nonfinite values or negative dimensions,
         *         leaving existing bounds unchanged; otherwise true.
         * @note Free widgets resolve this request through anchor and pivot.
         *       Containers assign a separate rectangle during layout.
         *       Does not overwrite an existing arrangement immediately.
         */
        bool SetBounds(const Rect& bounds);

        /**
         * @brief Returns authored pixel offset and size before anchor/pivot resolution.
         */
        [[nodiscard]] const Rect& GetLocalBounds() const noexcept { return m_LocalBounds; }

        /**
         * @brief Assigns a container-controlled parent-relative rectangle.
         * @return False for nonfinite values or negative dimensions.
         * @note Preserves authored bounds, anchor, and pivot.
         *       Arranged bounds bypass anchor/pivot resolution.
         *       Invalidates paint and interaction without requesting another layout.
         * @pre Called by the parent during arrangement.
         */
        bool SetArrangedBounds(const Rect& bounds);

        /**
         * @brief Removes the container-assigned rectangle.
         * @note Restores authored anchor/offset/pivot resolution.
         * @pre Called during arrangement or hierarchy ownership changes.
         */
        void ClearArrangedBounds() noexcept;

        /**
         * @brief Sets normalized parent anchor and widget pivot coordinates.
         * @return False for nonfinite input, preserving existing placement.
         *         Finite values are clamped to [0, 1].
         * @note Bounds position becomes the pixel offset from the anchor.
         *       Size remains explicit. Parentless widgets ignore anchor and pivot.
         *       Container-assigned bounds take precedence without modifying these values.
         */
        bool SetAnchorAndPivot(const glm::vec2& anchor, const glm::vec2& pivot);
        [[nodiscard]] const glm::vec2& GetAnchor() const noexcept { return m_Anchor; }
        [[nodiscard]] const glm::vec2& GetPivot() const noexcept { return m_Pivot; }

        /**
         * @brief Sets alignment within container-allocated space.
         * @note VerticalBox uses horizontal alignment, HorizontalBox uses
         *       vertical alignment, and Button uses both.
         *       Base Widget placement continues to use anchor and pivot.
         *       Does not change the measured desired size.
         */
        void SetAlignment(Alignment horizontal, Alignment vertical) noexcept;
        [[nodiscard]] Alignment GetHorizontalAlignment() const noexcept { return m_HorizontalAlignment; }
        [[nodiscard]] Alignment GetVerticalAlignment() const noexcept { return m_VerticalAlignment; }

        /**
         * @brief Resolves container-assigned or authored geometry into surface bounds.
         * @note Arranged bounds are already parent-relative and bypass anchor/pivot.
         *       Otherwise resolves the authored offset, anchor, and pivot.
         *       Computes on demand without scaling or rotation.
         * @pre Intermediate and final resolved coordinates must remain finite.
         */
        [[nodiscard]] Rect GetSurfaceBounds() const noexcept;

        /**
         * @brief Clips this widget and its descendants to this widget's bounds.
         * @note Does not change measurement or arrangement.
         *       Ancestor clips still apply when this setting is disabled.
         */
        void SetClipToBounds(bool enabled) noexcept;
        [[nodiscard]] bool GetClipToBounds() const noexcept { return m_ClipToBounds; }

        /**
         * @brief Resolves the intersection of enabled clips on this ancestor chain.
         * @return No value when no widget enables clipping; otherwise the
         *         effective surface-space rectangle, which may be empty.
         * @pre Resolved bounds and their maximum coordinates must remain finite.
         */
        [[nodiscard]] std::optional<Rect> GetSurfaceClip() const noexcept;

        /**
         * @brief Enables or disables interaction for this subtree.
         * @note Disabling immediately cancels hover and pending presses.
         *       Preserves layout, visibility, and hit-test geometry.
         *       Re-enabling does not restore a cancelled press.
         */
        void SetEnabled(bool enabled) noexcept;

        /**
         * @brief Returns this widget's local enabled setting.
         */
        [[nodiscard]] bool IsEnabled() const noexcept { return m_Enabled; }

        /**
         * @brief Returns true when this widget and all ancestors are enabled.
         */
        [[nodiscard]] bool IsEffectivelyEnabled() const noexcept;

        /**
         * @brief Sets participation in hit testing without changing painting.
         * @note ChildrenOnly skips this widget but still searches descendants.
         *       None skips the entire subtree.
         */
        void SetHitTestMode(HitTestMode mode) noexcept;
        [[nodiscard]] HitTestMode GetHitTestMode() const noexcept { return m_HitTestMode; }

        /**
         * @brief Finds the topmost participating widget at a surface-space point.
         * @return Borrowed widget pointer, or null when nothing is hit.
         * @note Searches children in reverse painting order before this widget.
         *       Enabled clips on this widget and its ancestors restrict hits.
         *       Minimum edges are included; maximum edges are excluded.
         *       Nonfinite points produce no hit.
         * @note Disabled widgets retain their hit-test participation so
         *       disabling a control does not make its geometry click-through.
         * @pre The hierarchy must remain unchanged during the query.
         * @pre Resolved bounds and their maximum coordinates must remain finite.
         */
        [[nodiscard]] Widget* HitTest(const glm::vec2& surfacePosition) noexcept;

        /**
         * @brief Updates direct hover state throughout this subtree.
         * @param hoveredWidget Current hit-test result, or null to clear hover.
         * @note Call on the input root with a result from that root.
         *       Only the directly hit widget is hovered, not its ancestors.
         *       Stores no pointer and invalidates paint only when state changes.
         * @pre The hierarchy must remain unchanged during this call.
         */
        void UpdatePointerHover(const Widget* hoveredWidget) noexcept;
        [[nodiscard]] bool IsPointerHovered() const noexcept { return m_PointerHovered; }

        /**
         * @brief Updates primary-button ownership and identifies release-inside activation.
         * @param hoveredWidget Current hit-test result, or null.
         * @param primaryPressed True on the primary-button down transition.
         * @param primaryDown True while the primary button remains held.
         * @param primaryReleased True on the primary-button up transition.
         * @return Borrowed activation target, or null.
         * @note A press starts only on the directly hit widget.
         *       Release activates only when that widget is still the hit target.
         *       All false button values cancel without activation.
         *       None excludes and cancels the entire subtree.
         *       Does not invoke callbacks.
         * @pre Call on the input root without modifying the hierarchy during traversal.
         */
        Widget* UpdatePointerPress(const Widget* hoveredWidget, bool primaryPressed, bool primaryDown, bool primaryReleased = false) noexcept;
        [[nodiscard]] bool IsPointerPressed() const noexcept { return m_PointerPressed; }

        /**
         * @brief Performs this widget's activation behavior.
         * @note The base implementation does nothing.
         *       Call after input traversal finishes; exceptions propagate.
         */
        virtual void Activate();

        /**
         * @brief Transfers an exclusively owned subtree into this widget.
         * @param child Detached subtree to attach; must not be null.
         * @return Borrowed reference to the attached child.
         * @throws std::invalid_argument For null input or a hierarchy cycle.
         * @note Validation occurs before moving from the supplied parameter.
         *       Allocation exceptions propagate.
         * @note Never construct an owning pointer from a borrowed widget pointer.
         */
        Widget& AddChild(std::unique_ptr<Widget>&& child);

        /**
         * @brief Transfers ownership of a direct child to the caller.
         * @return Detached subtree, or null when child is not a direct child.
         * @note Detachment clears the child's parent pointer and preserves
         *       its descendants. Discarding the returned owner destroys
         *       that subtree and invalidates its borrowed references.
         */
        [[nodiscard]] std::unique_ptr<Widget> DetachChild(const Widget& child);

        /**
         * @brief Marks pending work on this widget and its ancestors.
         * @note Layout also invalidates paint and interaction.
         */
        void Invalidate(InvalidationReason reason) noexcept;
        [[nodiscard]] const InvalidationState& GetInvalidationState() const noexcept { return m_Invalidation; }

        /**
         * @brief Acknowledges one category on this widget and its descendants.
         * @note The owner calls this only after processing that category.
         *       Does not clear other categories or update ancestor flags.
         */
        void ClearInvalidation(InvalidationReason reason) noexcept;

        /**
         * @brief Measures descendants before calculating this widget's preferred size.
         * @param context CPU resources borrowed only during this call.
         * @note Stores an unconstrained size without assigning geometry or clearing flags.
         *       Allocation and measurement exceptions propagate.
         * @pre The hierarchy and authored layout inputs must remain unchanged.
         */
        void Measure(const LayoutContext& context = {});

        /**
         * @brief Returns the preferred size from the last successful measurement.
         * @pre Measure() must have completed after authored layout inputs changed.
         */
        [[nodiscard]] const glm::vec2& GetDesiredSize() const noexcept { return m_DesiredSize; }

        /**
         * @brief Arranges direct children, then recursively arranges their descendants.
         * @note Does not change this widget's own bounds or clear invalidation flags.
         * @pre The hierarchy must remain unchanged during traversal.
         * @pre Measure() must have completed for this subtree after layout inputs changed.
         */
        void ArrangeChildren();

        /**
         * @brief Appends painting commands for this widget and its descendants.
         * @param commands Caller-owned list; existing commands are preserved.
         * @note Paints this widget first, then children in insertion order.
         * @note Applies the effective surface clip to commands appended by OnPaint().
         *       OnPaint() must leave command Clip fields unset.
         * @note Produces logical UI coordinates before Application scaling.
         * @pre The hierarchy and paint-affecting widget state must not be modified during painting.
         */
        void Paint(PaintList& commands) const;

    protected:
        /**
         * @brief Calculates this widget's unconstrained preferred size.
         * @return Finite, nonnegative dimensions.
         * @note Descendants have already been measured.
         *       The base implementation returns the authored explicit size.
         */
        virtual glm::vec2 OnMeasure(const LayoutContext& context) const;

        /**
         * @brief Assigns geometry to direct children.
         * @note The base implementation uses measured sizes and authored placement.
         *       Containers override this to assign their own child rectangles.
         *       Must not change the hierarchy or ancestor geometry.
         */
        virtual void OnArrangeChildren();

        /**
         * @brief Appends this widget's own visual commands.
         * @note The base implementation draws nothing.
         *       Child traversal is performed by Paint().
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

        bool m_Enabled = true;
        bool m_ClipToBounds = false;
        HitTestMode m_HitTestMode = HitTestMode::ChildrenOnly;
        bool m_PointerHovered = false;
        bool m_PointerPressed = false;
        InvalidationState m_Invalidation{};
        std::vector<std::unique_ptr<Widget>> m_Children;
    };
}

#pragma once

#include <cstddef>
#include <memory>
#include <vector>

#include "Geometry.h"
#include "Quiet/UI/PaintCommands.h"

namespace Quiet::UI
{
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
         * @brief Sets this widget's parent-relative pixel bounds.
         * @return False for nonfinite values or negative dimensions,
         *         leaving existing bounds unchanged; otherwise true.
         * @note A parentless widget uses surface-relative position.
         *       Does not resize or reposition children.
         */
        bool SetBounds(const Rect& bounds);

        /**
         * @brief Returns this widget's stored parent-relative bounds.
         */
        [[nodiscard]] const Rect& GetLocalBounds() const noexcept { return m_LocalBounds; }

        /**
         * @brief Resolves bounds into surface coordinates.
         * @note Adds ancestor positions without scaling or rotation.
         *       Computes on demand; retains no cache.
         * @pre Accumulated positions must remain representable as floats.
         */
        [[nodiscard]] Rect GetSurfaceBounds() const noexcept;

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
         * @brief Appends painting commands for this widget and its descendants.
         * @param commands Caller-owned list; existing commands are preserved.
         * @note Paints this widget first, then children in insertion order.
         * @pre The hierarchy and paint-affecting widget state must not be modified during painting.
         */
        void Paint(PaintList& commands) const;

    protected:
        /**
         * @brief Appends this widget's own visual commands.
         * @note The base implementation draws nothing.
         *       Child traversal is performed by Paint().
         */
        virtual void OnPaint(PaintList& commands) const;

    private:
        Widget* m_Parent = nullptr;
        Rect m_LocalBounds{};
        InvalidationState m_Invalidation{};
        std::vector<std::unique_ptr<Widget>> m_Children;
    };
}

#pragma once

#include <string>

#include <glm/vec4.hpp>

#include "Quiet/UI/Widget.h"

namespace Quiet::UI
{
    /**
     * @brief Paints a texture reference over explicit widget bounds.
     * @ingroup quiet_renderer
     *
     * Uses the complete texture stretched over the bounds.
     * Does not preserve aspect ratio or infer natural dimensions.
     * An empty texture ID or zero dimension emits no command.
     *
     * Owns no texture, descriptor, or GPU resource.
     */
    class Image final : public Widget
    {
    public:
        /**
         * @brief Sets the texture-library identity used during painting.
         * @note An empty ID clears the image.
         *       Does not load a resource or validate its availability.
         */
        void SetTextureID(std::string textureID);
        [[nodiscard]] const std::string& GetTextureID() const noexcept { return m_TextureID; }

        /**
         * @brief Sets straight-alpha linear RGBA tint.
         * @return False for nonfinite input, preserving the previous tint.
         *         Finite channels are clamped to [0, 1].
         */
        bool SetTint(const glm::vec4& tint);
        [[nodiscard]] const glm::vec4& GetTint() const noexcept { return m_Tint; }

    protected:
        /**
         * @pre Surface bounds and their maximum coordinates must remain
         *      representable as finite floats.
         */
        void OnPaint(PaintList& commands) const override;

    private:
        std::string m_TextureID;
        glm::vec4 m_Tint{1.0f};
    };
}
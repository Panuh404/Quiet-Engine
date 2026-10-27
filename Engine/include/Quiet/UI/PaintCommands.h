#pragma once

#include <string>
#include <variant>
#include <vector>
#include <optional>

#include <glm/vec2.hpp>
#include <glm/vec4.hpp>

#include "Quiet/UI/Geometry.h"

namespace Quiet::UI
{
    /**
     * @brief Describes a solid rectangle independently of the renderer backend.
     * @ingroup quiet_renderer
     *
     * Min and Max are resolved surface-space pixel coordinates.
     * The origin is top-left; positive X points right and positive Y down.
     * Color uses straight-alpha linear RGBA.
     *
     * Coordinates must be finite, with Max greater than Min on both axes.
     * Color channels must be finite and within [0, 1].
     *
     * Owns its values and contains no resource references or GPU state.
     * Clipping and draw ordering are supplied by the consuming paint path.
     */
    struct SolidRectCommand
    {
        glm::vec2 Min{0.0f};
        glm::vec2 Max{0.0f};
        glm::vec4 Color{1.0f};

        /**
         * @brief Optional surface-space clip rectangle.
         * @note An absent clip permits the complete target surface.
         *       An empty clip suppresses drawing.
         */
        std::optional<Rect> Clip;
    };

    /**
     * @brief Describes UTF-8 text independently of the renderer backend.
     * @ingroup quiet_renderer
     *
     * Origin is the resolved surface-space pixel position of the
     * logical text box's top-left. Positive X points right and Y down.
     * PixelHeight is the requested ascender-to-descender height.
     * Color uses straight-alpha linear RGBA.
     *
     * Origin must be finite and PixelHeight finite and positive.
     * Color channels must be finite and within [0, 1].
     *
     * Uses the renderer's current default runtime font.
     * Malformed UTF-8 is rejected by text layout.
     *
     * Owns its content and values; retains no font or GPU resources.
     * Clipping and draw ordering are supplied by the consuming paint path.
     */
    struct TextCommand
    {
        std::string Content;
        glm::vec2 Origin{0.0f};
        float PixelHeight = 32.0f;
        glm::vec4 Color{1.0f};

        /**
         * @brief Optional surface-space clip rectangle.
         * @note An absent clip permits the complete target surface.
         *       An empty clip suppresses drawing.
         */
        std::optional<Rect> Clip;
    };

    /**
     * @brief Describes a textured rectangle independently of the backend.
     * @ingroup quiet_renderer
     *
     * Min and Max are resolved surface-space pixel coordinates.
     * TextureID identifies an entry in the renderer's texture library.
     * Project textures use their existing asset IDs.
     *
     * Draws the complete texture stretched over the rectangle.
     * Tint uses straight-alpha linear RGBA.
     *
     * Coordinates must be finite, with Max greater than Min on both axes.
     * Tint channels must be finite and within [0, 1].
     *
     * Owns its ID and values; does not own or keep a texture loaded.
     */
    struct ImageCommand
    {
        glm::vec2 Min{0.0f};
        glm::vec2 Max{0.0f};
        std::string TextureID;
        glm::vec4 Tint{1.0f};

        /**
         * @brief Optional surface-space clip rectangle.
         * @note An absent clip permits the complete target surface.
         *       An empty clip suppresses drawing.
         */
        std::optional<Rect> Clip;
    };

    /**
     * @brief Stores one backend-independent painting operation.
     * @ingroup quiet_renderer
     */
    using PaintCommand = std::variant<SolidRectCommand, TextCommand, ImageCommand>;

    /**
     * @brief Owns painting operations in back-to-front submission order.
     * @ingroup quiet_renderer
     *
     * Consumers execute commands in vector order without sorting by type.
     * Later commands paint over earlier commands where their geometry overlaps.
     * Text and image commands own their strings.
     * The list contains no GPU resources.
     */
    using PaintList = std::vector<PaintCommand>;
}
#include "Quiet/UI/PaintCommands.h"

#include <cmath>
#include <stdexcept>
#include <type_traits>

namespace Quiet::UI
{
    void ScalePaintList(PaintList& commands, const float scale)
    {
        if (!std::isfinite(scale) || scale <= 0.0f)
        {
            throw std::invalid_argument("UI paint scale must be finite and positive.");
        }
        if (scale == 1.0f)
        {
            return;
        }

        for (PaintCommand& command : commands)
        {
            std::visit([scale](auto& operation)
            {
                using Operation = std::remove_cvref_t<decltype(operation)>;

                if constexpr (std::is_same_v<Operation, TextCommand>)
                {
                    operation.Origin *= scale;
                    operation.PixelHeight *= scale;
                }
                else
                {
                    operation.Min *= scale;
                    operation.Max *= scale;
                }

                if (operation.Clip)
                {
                    operation.Clip->Position *= scale;
                    operation.Clip->Size *= scale;
                }
            }, command);
        }
    }
}
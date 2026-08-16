#pragma once

#include "ImGuiInspectorContext.h"
#include "ImGuiInspectorState.h"

namespace Quiet::Renderer::UI::Inspector
{
    void DrawInspectorPanel(InspectorContext& context, InspectorState& state);
}
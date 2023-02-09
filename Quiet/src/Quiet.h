#pragma once

//-----------------------------------------------------------------------------
// FOR USE BY QUIET APPLICATIONS
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// [SECTION] Core
//-----------------------------------------------------------------------------
#include "Quiet/Core/Core.h"

#include "Quiet/Core/Application.h"
#include "Quiet/Core/Layer.h"
#include "Quiet/Core/Log.h"
#include "Quiet/Core/Timestep.h"

#include "Quiet/ImGui/ImGuiLayer.h"

//-----------------------------------------------------------------------------
// [SECTION] Input
//-----------------------------------------------------------------------------
#include "Quiet/Core/Input.h"
#include "Quiet/Core/KeyCodes.h"
#include "Quiet/Core/MouseCodes.h"


//-----------------------------------------------------------------------------
// [SECTION] Renderer Area
//-----------------------------------------------------------------------------
#include "Quiet/Renderer/Renderer.h"
#include "Quiet/Renderer/RendererCommand.h"

#include "Quiet/Renderer/Buffer.h"
#include "Quiet/Renderer/Shader.h"
#include "Quiet/Renderer/Texture.h"
#include "Quiet/Renderer/VertexArray.h"

#include "Quiet/Renderer/CameraOrthographic.h"

//-----------------------------------------------------------------------------
// [Entry Point]
//-----------------------------------------------------------------------------
#include "Quiet/Core/EntryPoint.h"
#include "Renderer.h"
#include "GraphicsPlatformInterface.h"
#include "Direct3D12/D3D12Interface.h"

namespace quiet::graphics
{
	namespace
	{
		platform_interface gfx{};

		bool set_platform_interface(graphics_platform platform)
		{
			switch (platform)
			{
			case direct3d12:
				d3d12::get_platform_interface(gfx);
				break;
			default:
				return false;
			}
			return true;
		}
	}

	bool initialize(graphics_platform platform)
	{
		return set_platform_interface(platform) && gfx.initialize();
	}

	void shutdown()
	{
		gfx.shutdown();
	}
}

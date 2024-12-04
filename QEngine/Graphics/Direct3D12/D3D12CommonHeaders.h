#pragma once
#include "CommonHeaders.h"
#include "Graphics/Renderer.h"

// DX12
#include <dxgi1_6.h>
#include <d3d12.h>
#include <wrl.h>

// Lib
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3d12.lib")

namespace quiet::graphics::d3d12
{
	constexpr u32 frame_buffer_count{ 3 };
}

// Asset that COM call to D3D API Succeeded
#ifdef _DEBUG
	#ifndef DXCall
		#define DXCall(x)								\
		if(FAILED(x)) {                                 \
		    char line_number[32];                       \
		    sprintf_s(line_number, "%u", __LINE__);     \
		    OutputDebugStringA("Error in: ");           \
		    OutputDebugStringA(__FILE__);               \
		    OutputDebugStringA("\nLine: ");             \
		    OutputDebugStringA(line_number);            \
		    OutputDebugStringA("\n");                   \
		    OutputDebugStringA(#x);                     \
		    OutputDebugStringA("\n");                   \
		    __debugbreak();                             \
		}
	#endif
#else
	#ifndef DXCall
		#define DXCall(x)
	#endif
#endif


#ifdef _DEBUG
	// Sets the name of the COM object and outputs a debug string int Visual Studio's output panel.
	#define NAME_D3D12_OBJECT(obj, name) obj->SetName(name); OutputDebugString(L"::D3D12 Object Created: "); OutputDebugString(name); OutputDebugString(L"\n");
	#define NAME_D3D12_OBJECT_INDEXED(obj, n, name)     \
	{                                                   \
	wchar_t full_name[128];                             \
	if (swprintf_s(full_name, L"%s[%u]", name, n) >0 ){ \
	    obj->SetName(full_name);                        \
	    OutputDebugString(L"::D3D12 Object Created: "); \
	    OutputDebugString(full_name);                   \
	    OutputDebugString(L"\n");                       \
	}}
#else
	#define NAME_D3D12_OBJECT(x, name)
#endif // _DEBUG
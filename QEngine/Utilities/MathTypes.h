#pragma once
#include "CommonHeaders.h"

namespace quiet::math
{
	constexpr float pi = 3.141592653589793238462643383279502884197169399375105820974944592307816406286208998628034825342f;
	constexpr float epsilon = 1e-5f;

#if defined(_WIN64)
	// Float Vectors
	using v2	= DirectX::XMFLOAT2;
	using v3	= DirectX::XMFLOAT3;
	using v4	= DirectX::XMFLOAT4;

	// Float Vectors Aligned
	using v2a	= DirectX::XMFLOAT2A;
	using v3a	= DirectX::XMFLOAT3A;
	using v4a	= DirectX::XMFLOAT4A;

	// Unsigned Integers Vectors
	using u32v2 = DirectX::XMUINT2;
	using u32v3 = DirectX::XMUINT3;
	using u32v4 = DirectX::XMUINT4;

	// Signed Integers Vectors
	using s32v2 = DirectX::XMINT2;
	using s32v3 = DirectX::XMINT3;
	using s32v4 = DirectX::XMINT4;

	// Matrices
	using m3x3	= DirectX::XMFLOAT3X3;	// DirectXMath doesn't have aligned 3x3 matrices
	using m4x4	= DirectX::XMFLOAT4X4;
	using m4x4a	= DirectX::XMFLOAT4X4A;
#endif


}
#pragma once

#include <string>

#include "Quiet/Core/Core.h"

namespace Quiet
{
	//-----------------------------------------------------------------------------
	// [CLASS] Texture
	//-----------------------------------------------------------------------------
	class Texture
	{
	public:

		virtual ~Texture() = default;

		virtual uint32_t GetWidth() const = 0;
		virtual uint32_t GetHeight() const = 0;

		virtual void Bind(uint32_t slot = 0) const = 0;
	};

	//-----------------------------------------------------------------------------
	// [SUBCLASS] Texture2D
	//-----------------------------------------------------------------------------
	class Texture2D : public Texture
	{
	public:
		static std::shared_ptr<Texture2D> Create(const std::string& path);
	};
}
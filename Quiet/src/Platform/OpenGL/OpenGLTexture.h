#pragma once

#include "Quiet/Renderer/Texture.h"

namespace Quiet
{
	class OpenGLTexture2D : public Texture2D
	{
	public:
		OpenGLTexture2D(const std::string& path);
		virtual ~OpenGLTexture2D();

		uint32_t GetWidth() const override { return m_Width; }
		uint32_t GetHeight() const override { return m_Height; }

		void Bind(uint32_t slot = 0) const override;

	private:
		uint32_t m_RendererID;
		std::string m_Path;
		uint32_t m_Width;
		uint32_t m_Height;
	};
}
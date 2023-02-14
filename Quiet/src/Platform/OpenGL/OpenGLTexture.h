#pragma once

#include "Quiet/Renderer/Texture.h"
#include <glad/glad.h>

namespace Quiet
{
	//-----------------------------------------------------------------------------
	// [CLASS] Texture2D -- API: OpenGL
	//-----------------------------------------------------------------------------
	class OpenGLTexture2D : public Texture2D
	{
	public:
		OpenGLTexture2D(uint32_t width, uint32_t height);
		OpenGLTexture2D(const std::string& path);
		virtual ~OpenGLTexture2D();

		uint32_t GetWidth() const override { return m_Width; }
		uint32_t GetHeight() const override { return m_Height; }

		void SetData(void* data, uint32_t size) override;
		void Bind(uint32_t slot = 0) const override;

	private:
		uint32_t m_RendererID;
		std::string m_Path;

		uint32_t m_Width;
		uint32_t m_Height;
		GLenum m_InternalFormat, m_DataFormat;
	};
}
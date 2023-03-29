#pragma once

#include "Quiet/Renderer/Framebuffer.h"

namespace Quiet
{
	class OpenGLFramebuffer : public Framebuffer
	{
	public:
		OpenGLFramebuffer(const FramebufferSpecification& spec);
		virtual ~OpenGLFramebuffer();

		void Invalidate();

		void Bind() override;
		void Unbind() override;

		uint32_t GetColorAttachmentRendererID() const override { return m_ColorAttachment; }
		const FramebufferSpecification& GetSpecification() const override { return m_Specification; }

	private:
		uint32_t m_RendererID;
		uint32_t m_ColorAttachment;
		uint32_t m_DepthAttachment;

		FramebufferSpecification m_Specification;
	};
}
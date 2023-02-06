#pragma once

namespace Quiet
{
	class Shader
	{
	public:
		Shader(const std::string& vertexSrc, const std::string& fragmentStr);
		~Shader();

		void Bind() const;
		void Unbind() const;

	private:
		uint32_t m_RendererID;

	};
}
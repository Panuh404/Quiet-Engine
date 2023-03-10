#pragma once

#include "Quiet/Renderer/Shader.h"

#include <glm/glm.hpp>

// TODO: Remove
typedef unsigned int GLenum;

namespace Quiet
{
	//-----------------------------------------------------------------------------
	// [CLASS] Shader -- API: OpenGL
	//-----------------------------------------------------------------------------
	class OpenGLShader : public Shader
	{
	public:
		OpenGLShader(const std::string& filepath);
		OpenGLShader(const std::string& name, const std::string& vertexSrc, const std::string& fragmentSrc);
		virtual ~OpenGLShader();

		void Bind() const override;
		void Unbind() const override;

		const std::string& GetName() const override { return m_Name; }

		// Uniforms References
		void SetInt(const std::string& name, int value) override						{ QT_PROFILE_FUNCTION(); UploadUniformInt(name, value);				}
		void SetIntArr(const std::string& name, int* values, uint32_t count) override	{ QT_PROFILE_FUNCTION(); UploadUniformIntArr(name, values, count);	}
		void SetFloat(const std::string& name, float values) override					{ QT_PROFILE_FUNCTION(); UploadUniformFloat(name, values);			}
		void SetFloat2(const std::string& name, const glm::vec2& values) override		{ QT_PROFILE_FUNCTION(); UploadUniformFloat2(name, values);			}
		void SetFloat3(const std::string& name, const glm::vec3& values) override		{ QT_PROFILE_FUNCTION(); UploadUniformFloat3(name, values);			}
		void SetFloat4(const std::string& name, const glm::vec4& values) override		{ QT_PROFILE_FUNCTION(); UploadUniformFloat4(name, values);			}
		void SetMat3(const std::string& name, const glm::mat3& matrix) override			{ QT_PROFILE_FUNCTION(); UploadUniformMat3(name, matrix);			}
		void SetMat4(const std::string& name, const glm::mat4& matrix) override			{ QT_PROFILE_FUNCTION(); UploadUniformMat4(name, matrix);			}

		// Uniforms Handle
		void UploadUniformInt	(const std::string& name, int value);
		void UploadUniformIntArr(const std::string& name, int* values, uint32_t count);
		void UploadUniformFloat	(const std::string& name, float value);
		void UploadUniformFloat2(const std::string& name, const glm::vec2& values);
		void UploadUniformFloat3(const std::string& name, const glm::vec3& values);
		void UploadUniformFloat4(const std::string& name, const glm::vec4& values);
		void UploadUniformMat3	(const std::string& name, const glm::mat3& matrix);
		void UploadUniformMat4	(const std::string& name, const glm::mat4& matrix);

	private:
		uint32_t m_RendererID;
		std::string m_Name;

		std::string ReadFile(const std::string filepath);
		std::unordered_map<GLenum, std::string> PreProcess(const std::string source);
		void Compile(const std::unordered_map<GLenum, std::string>& shaderSources);
	};
}
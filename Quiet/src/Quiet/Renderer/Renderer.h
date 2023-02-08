#pragma once

#include "Quiet/Renderer/RendererCommand.h"
#include "Quiet/Renderer/CameraOrthographic.h"
#include "Quiet/Renderer/Shader.h"

namespace Quiet
{
	class Renderer
	{
	public:
		static void BeginScene(CameraOrthographic& camera);
		static void EndScene();

		static void Submit(const std::shared_ptr<Shader>& shader, const std::shared_ptr<VertexArray>& vertexArray);

		inline static RendererAPI::API GetAPI() { return RendererAPI::GetAPI(); }

	private:
		struct SceneData
		{
			glm::mat4 ViewProjectionMatrix;
		};
		static SceneData* s_SceneData;
	};
}
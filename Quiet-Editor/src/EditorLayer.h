#pragma once

#include "Quiet.h"

namespace Quiet
{
	class EditorLayer : public Layer
	{
	public:
		EditorLayer();
		virtual ~EditorLayer() = default;

		void OnAttach() override;
		void OnDetach() override;

		void OnUpdate(Timestep dt) override;
		void OnImGuiRender() override;
		void OnEvent(Event& event) override;

	private:
		OrthographicCameraController m_CameraController;

		// TEMP
		std::shared_ptr<VertexArray> m_SquareVA;
		std::shared_ptr<Shader> m_FlatColorShader;
		std::shared_ptr<Framebuffer> m_Framebuffer;

		std::shared_ptr<Texture2D> m_CheckerboardTexture;

		glm::vec4 m_SquareColor = { 0.2f, 0.3f, 0.8f, 1.0f };
	};	
}

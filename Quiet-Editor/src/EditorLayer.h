#pragma once

#include "Quiet.h"

namespace Quiet
{
	class EditorLayer : public Layer
	{
	public:
		EditorLayer();
		virtual ~EditorLayer() = default;

		virtual void OnAttach() override;
		virtual void OnDetach() override;

		void OnUpdate(Timestep dt) override;
		virtual void OnImGuiRender() override;
		void OnEvent(Event& event) override;

	private:
		OrthographicCameraController m_CameraController;

		// TEMP
		std::shared_ptr<VertexArray> m_SquareVA;
		std::shared_ptr<Shader> m_FlatColorShader;
		std::shared_ptr<Framebuffer> m_Framebuffer;

		std::shared_ptr<Texture2D> m_CheckerboardTexture;

		bool m_ViewportFocused = false;
		bool m_ViewportHovered = false;

		glm::vec2 m_ViewportSize = { 0.0f,0.0f };
		glm::vec4 m_SquareColor = { 0.2f, 0.3f, 0.8f, 1.0f };
	};	
}

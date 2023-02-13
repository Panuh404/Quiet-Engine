#pragma once

#include "Quiet.h"

class Sandbox2D : public Quiet::Layer
{
public:
	Sandbox2D();
	virtual ~Sandbox2D() = default;

	void OnAttach() override;
	void OnDetach() override;

	void OnUpdate(Quiet::Timestep dt) override;
	void OnImGuiRender() override;
	void OnEvent(Quiet::Event& event) override;

private:
	Quiet::OrthographicCameraController m_CameraController;

	// TEMP
	std::shared_ptr<Quiet::VertexArray> m_SquareVA;
	std::shared_ptr<Quiet::Shader> m_FlatColorShader;

	glm::vec4 m_SquareColor = { 0.2f, 0.3f, 0.8f, 1.0f };
};

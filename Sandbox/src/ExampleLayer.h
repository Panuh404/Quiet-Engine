#pragma once

#include "Quiet.h"

class ExampleLayer : public Quiet::Layer
{
public:
	ExampleLayer();
	virtual ~ExampleLayer() = default;

	void OnAttach() override;
	void OnDetach() override;

	void OnUpdate(Quiet::Timestep ts) override;
	void OnImGuiRender() override;
	void OnEvent(Quiet::Event& e) override;

private:
	Quiet::ShaderLibrary m_ShaderLibrary;
	std::shared_ptr<Quiet::Shader> m_FlatColorShader;
	std::shared_ptr<Quiet::VertexArray> m_SquareVA;

	std::shared_ptr<Quiet::Texture2D> m_Texture2D;
	std::shared_ptr<Quiet::Texture2D> m_TexFace;

	Quiet::OrthographicCameraController m_CameraController;
	glm::vec4 m_Color = { 0.8f, 0.2f, 0.3f, 1.0f };

	Quiet::Timestep m_DebugTS;
};
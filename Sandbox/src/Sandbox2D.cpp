#include "Sandbox2D.h"
#include "ImGui/imgui.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.inl>

Sandbox2D::Sandbox2D() : Layer("Sandbox2D"), m_CameraController(1280.0f / 720.0f) {}

void Sandbox2D::OnAttach()
{
	QT_PROFILE_FUNCTION();
	m_CheckerboardTexture = Quiet::Texture2D::Create("res/textures/Checkerboard.png");
}

void Sandbox2D::OnDetach()
{
	QT_PROFILE_FUNCTION();
}

void Sandbox2D::OnUpdate(Quiet::Timestep dt)
{
	QT_PROFILE_SCOPE("Sandbox2D::OnUpdate");

	// Update Camera
	m_CameraController.OnUpdate(dt);

	// Render Setup
	{
		QT_PROFILE_SCOPE("Renderer::Prep");
		Quiet::RendererCommand::SetClearColor({ 0.1f,0.1f,0.1f,1 });
		Quiet::RendererCommand::Clear();
	}
	// Draw
	{
		static float rotation = 0.0f;
		rotation += dt * 50.0f;

		QT_PROFILE_SCOPE("Renderer::Draw");
		Quiet::Renderer2D::BeginScene(m_CameraController.GetCamera());
		Quiet::Renderer2D::DrawRotatedQuad({ 1.0f, 0.0f }, { 0.8f, 0.8f }, -45.0f, { 0.8f, 0.2f, 0.3f, 1.0f });
		Quiet::Renderer2D::DrawQuad({ -1.0f, 0.0f }, { 0.8f, 0.8f }, { m_SquareColor});
		Quiet::Renderer2D::DrawQuad({ 0.5f, -0.5f }, { 0.5f, 0.75f }, { 0.2f, 0.3f, 0.8f, 1.0f });
		Quiet::Renderer2D::DrawQuad({ 0.0f, 0.0f, -0.1f }, { 10.0f, 10.0f }, m_CheckerboardTexture, 10.0f);
		Quiet::Renderer2D::DrawRotatedQuad({ -2.0f, 0.0f, 0.0f }, { 1.0f, 1.0f }, rotation, m_CheckerboardTexture, 20.0f);
		Quiet::Renderer2D::EndScene();
	}
}

void Sandbox2D::OnImGuiRender()
{
	QT_PROFILE_FUNCTION();
	ImGui::Begin("Settings");

	ImGui::TextColored({ 0.3f, 0.4f, 0.8f, 1.0f }, "Profiling");
	ImGui::Separator();

	ImGui::TextColored({ 0.3f, 0.4f, 0.8f, 1.0f }, "Colors");
	ImGui::Separator();
	ImGui::ColorEdit4("Square Color", glm::value_ptr(m_SquareColor));
	
	ImGui::End();
}

void Sandbox2D::OnEvent(Quiet::Event& event)
{
	m_CameraController.OnEvent(event);
}

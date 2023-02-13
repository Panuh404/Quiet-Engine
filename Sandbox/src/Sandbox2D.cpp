#include "Sandbox2D.h"
#include "ImGui/imgui.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.inl>

#include "Platform/OpenGL/OpenGLShader.h"

Sandbox2D::Sandbox2D() : Layer("Sandbox2D"), m_CameraController(1280.0f / 720.0f) {}

void Sandbox2D::OnAttach()
{
	// Vertex Array
	m_SquareVA = Quiet::VertexArray::Create();

	float squareVertices[5 * 4] = {
			-0.5f, -0.5f, 0.0f,
			 0.5f, -0.5f, 0.0f,
			 0.5f,  0.5f, 0.0f,
			-0.5f,  0.5f, 0.0f
	};

	// Vertex Buffer
	std::shared_ptr<Quiet::VertexBuffer> squareVB;
	squareVB = Quiet::VertexBuffer::Create(squareVertices, sizeof(squareVertices));
	squareVB->SetLayout({
		{ Quiet::ShaderDataType::Float3, "a_Position"}
	});
	m_SquareVA->AddVertexBuffer(squareVB);

	uint32_t squareIndices[6] = { 0, 1, 2, 2, 3, 0 };

	// Index Buffer
	std::shared_ptr<Quiet::IndexBuffer> squareIB;
	squareIB = Quiet::IndexBuffer::Create(squareIndices, sizeof(squareIndices) / sizeof(uint32_t));
	m_SquareVA->SetIndexBuffer(squareIB);

	m_FlatColorShader = Quiet::Shader::Create("res/shaders/flatColor.shader");
}

void Sandbox2D::OnDetach() {}

void Sandbox2D::OnUpdate(Quiet::Timestep dt)
{
	// Update Camera
	m_CameraController.OnUpdate(dt);

	// Render
	Quiet::RendererCommand::SetClearColor({ 0.1f,0.1f,0.1f,1 });
	Quiet::RendererCommand::Clear();

	Quiet::Renderer::BeginScene(m_CameraController.GetCamera());

	std::dynamic_pointer_cast<Quiet::OpenGLShader>(m_FlatColorShader)->Bind();
	std::dynamic_pointer_cast<Quiet::OpenGLShader>(m_FlatColorShader)->UploadUniformFloat4("u_Color", m_SquareColor);
	const glm::mat4 transform = glm::scale(glm::mat4(1.0f), glm::vec3(1.5f));
	Quiet::Renderer::Submit(m_FlatColorShader, m_SquareVA, transform);

	Quiet::Renderer::EndScene();
}

void Sandbox2D::OnImGuiRender()
{
	ImGui::Begin("Settings");
	ImGui::ColorEdit4("Square Color", glm::value_ptr(m_SquareColor));
	ImGui::End();
}

void Sandbox2D::OnEvent(Quiet::Event& event)
{
	m_CameraController.OnEvent(event);
}

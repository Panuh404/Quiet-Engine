#include <Quiet.h>
#include "Quiet/Core/EntryPoint.h"

#include "ImGui/imgui.h"
#include "Quiet/Events/KeyEvent.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "Platform/OpenGL/OpenGLShader.h"
#include "Sandbox2D.h"

class TestLayer : public Quiet::Layer
{
public:
	TestLayer() : Layer("Test"), m_CameraController(1280.0f / 720.0f, true)
	{
		m_SquareVA = Quiet::VertexArray::Create();

		float squareVertices[5 * 4] = {
			// Square			// Texture
			-0.5f, -0.5f, 0.0f, 0.0f, 0.0f,
			 0.5f, -0.5f, 0.0f, 1.0f, 0.0f,
			 0.5f,  0.5f, 0.0f, 1.0f, 1.0f,
			-0.5f,  0.5f, 0.0f, 0.0f, 1.0f
		};

		std::shared_ptr<Quiet::VertexBuffer> squareVB;
		squareVB =Quiet::VertexBuffer::Create(squareVertices, sizeof(squareVertices));
		squareVB->SetLayout({
			{Quiet::ShaderDataType::Float3, "a_Position" },
			{Quiet::ShaderDataType::Float2, "a_TexCoord" }
			});
		m_SquareVA->AddVertexBuffer(squareVB);

		uint32_t squareIndices[6] = { 0, 1, 2, 2, 3, 0 };
		std::shared_ptr<Quiet::IndexBuffer> squareIB;
		squareIB =Quiet::IndexBuffer::Create(squareIndices, sizeof(squareIndices) / sizeof(uint32_t));
		m_SquareVA->SetIndexBuffer(squareIB);

		// Loaded in a Shader Variable
		m_FlatColorShader = Quiet::Shader::Create("res/shaders/flatColor.shader");

		// Loaded in the shader library
		auto textureShader = m_ShaderLibrary.Load("res/shaders/texture.shader");

		m_Texture2D = Quiet::Texture2D::Create("res/textures/Checkerboard.png");
		m_TexFace = Quiet::Texture2D::Create("res/textures/awesomeface.png");

		std::dynamic_pointer_cast<Quiet::OpenGLShader>(textureShader)->Bind();
		std::dynamic_pointer_cast<Quiet::OpenGLShader>(textureShader)->UploadUniformInt("u_Texture", 0);
	}

	void OnUpdate(Quiet::Timestep ts) override
	{
		// Set debug Timestep 
		m_DebugTS = ts;

		// Update
		m_CameraController.OnUpdate(ts);

		Quiet::RendererCommand::SetClearColor({ 0.1f, 0.1f, 0.1f, 1 });
		Quiet::RendererCommand::Clear();

		Quiet::Renderer::BeginScene(m_CameraController.GetCamera());

		glm::mat4 scale = glm::scale(glm::mat4(1.0f), glm::vec3(0.1f));

		std::dynamic_pointer_cast<Quiet::OpenGLShader>(m_FlatColorShader)->Bind();
		std::dynamic_pointer_cast<Quiet::OpenGLShader>(m_FlatColorShader)->UploadUniformFloat4("u_Color", m_Color);

		for (int y = -10; y < 11; y++)
		{
			for (int x = -10; x < 11; x++)
			{
				glm::vec3 pos(x * 0.11f, y * 0.11f, 0.0f);
				glm::mat4 transform = glm::translate(glm::mat4(1.0f), pos) * scale;
				Quiet::Renderer::Submit(m_FlatColorShader, m_SquareVA, transform);
			}
		}
		auto textureShader = m_ShaderLibrary.Get("texture");

		m_TexFace->Bind();
		Quiet::Renderer::Submit(textureShader, m_SquareVA, glm::scale(glm::mat4(1.0f), glm::vec3(1.5f)));

		Quiet::Renderer::EndScene();
	}

	void OnImGuiRender() override
	{
		ImGui::Begin("Test");
		ImGui::TextColored({ 0.3f, 0.4f, 0.8f, 1.0f }, "Debug");
		ImGui::Text("Deltatime: %f s (%f ms)", m_DebugTS, m_DebugTS.GetMilliseconds());
		ImGui::Separator();

		ImGui::Separator();
		ImGui::TextColored({ 0.3f, 0.4f, 0.8f, 1.0f }, "Box");
		ImGui::ColorEdit4("Square Color", glm::value_ptr(m_Color));
		ImGui::End();
	}

	void OnEvent(Quiet::Event& event) override
	{
		m_CameraController.OnEvent(event);
	}

private:
	Quiet::ShaderLibrary m_ShaderLibrary;
	std::shared_ptr<Quiet::Shader> m_FlatColorShader;
	std::shared_ptr<Quiet::VertexArray> m_SquareVA;

	std::shared_ptr<Quiet::Texture2D> m_Texture2D;
	std::shared_ptr<Quiet::Texture2D> m_TexFace;

	Quiet::OrthographicCameraController m_CameraController;
	glm::vec4 m_Color = { 0.8f, 0.2f, 0.3f, 1.0f };

	// Debug
	Quiet::Timestep m_DebugTS;
};

class Sandbox : public Quiet::Application
{
public:
	Sandbox()
	{
		// PushLayer(new TestLayer());
		PushLayer(new Sandbox2D());
	}

	~Sandbox()
	{
		
	}
};

Quiet::Application* Quiet::CreateApplication()
{
	return new Sandbox();
}

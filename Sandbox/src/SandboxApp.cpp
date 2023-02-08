#include <Quiet.h>

#include "ImGui/imgui.h"
#include "Quiet/Events/KeyEvent.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "Platform/OpenGL/OpenGLShader.h"

class TestLayer : public Quiet::Layer
{
public:
	TestLayer() : Layer("Test"), m_Camera(-1.6f, 1.6f, -0.9f, 0.9f), m_CameraPosition(0.0f)
	{
		m_VertexArray.reset(Quiet::VertexArray::Create());

		float vertices[3 * 7] = {
			-0.5f, -0.5f, 0.0f, 0.8f, 0.2f, 0.8f, 1.0f,
			 0.5f, -0.5f, 0.0f, 0.2f, 0.3f, 0.8f, 1.0f,
			 0.0f,  0.5f, 0.0f, 0.8f, 0.8f, 0.2f, 1.0f
		};

		std::shared_ptr<Quiet::VertexBuffer> vertexBuffer;
		vertexBuffer.reset(Quiet::VertexBuffer::Create(vertices, sizeof(vertices)));

		Quiet::BufferLayout layout = {
			{Quiet::ShaderDataType::Float3, "a_Position" },
			{Quiet::ShaderDataType::Float4, "a_Color" }
		};
		vertexBuffer->SetLayout(layout);
		m_VertexArray->AddVertexBuffer(vertexBuffer);

		uint32_t indices[3] = { 0, 1, 2 };
		std::shared_ptr<Quiet::IndexBuffer> indexBuffer;
		indexBuffer.reset(Quiet::IndexBuffer::Create(indices, sizeof(indices) / sizeof(uint32_t)));
		m_VertexArray->SetIndexBuffer(indexBuffer);

		m_SquareVA.reset(Quiet::VertexArray::Create());

		float squareVertices[3 * 4] = {
			-0.5f, -0.5f, 0.0f,
			 0.5f, -0.5f, 0.0f,
			 0.5f,  0.5f, 0.0f,
			-0.5f,  0.5f, 0.0f
		};

		std::shared_ptr<Quiet::VertexBuffer> squareVB;
		squareVB.reset(Quiet::VertexBuffer::Create(squareVertices, sizeof(squareVertices)));
		squareVB->SetLayout({
			{Quiet::ShaderDataType::Float3, "a_Position" }
			});
		m_SquareVA->AddVertexBuffer(squareVB);

		uint32_t squareIndices[6] = { 0, 1, 2, 2, 3, 0 };
		std::shared_ptr<Quiet::IndexBuffer> squareIB;
		squareIB.reset(Quiet::IndexBuffer::Create(squareIndices, sizeof(squareIndices) / sizeof(uint32_t)));
		m_SquareVA->SetIndexBuffer(squareIB);

		std::string flatColorShaderVertexSrc = R"(
			#version 330 core
			
			layout(location = 0) in vec3 a_Position;
			uniform mat4 u_ViewProjection;
			uniform mat4 u_Transform;
			out vec3 v_Position;
			void main()
			{
				v_Position = a_Position;
				gl_Position = u_ViewProjection * u_Transform * vec4(a_Position, 1.0);	
			}
		)";
		std::string flatColorShaderFragmentSrc = R"(
			#version 330 core
			
			layout(location = 0) out vec4 color;
			in vec3 v_Position;
			
			uniform vec4 u_Color;

			void main()
			{
				color = u_Color;
			}
		)";

		m_FlatColorShader.reset(Quiet::Shader::Create(flatColorShaderVertexSrc, flatColorShaderFragmentSrc));

		std::string posVertexSrc = R"(
			#version 330 core
			
			layout(location = 0) in vec3 a_Position;
			layout(location = 1) in vec4 a_Color;
			uniform mat4 u_ViewProjection;
			uniform mat4 u_Transform;
			out vec3 v_Position;
			out vec4 v_Color;
			void main()
			{
				v_Position = a_Position;
				v_Color = a_Color;
				gl_Position = u_ViewProjection * u_Transform * vec4(a_Position, 1.0);	
			}
		)";
		std::string posFragmentSrc = R"(
			#version 330 core
			
			layout(location = 0) out vec4 color;
			in vec3 v_Position;
			in vec4 v_Color;
			void main()
			{
				color = vec4(v_Position * 0.5 + 0.5, 1.0);
				color = v_Color;
			}
		)";

		m_PositionShader.reset( Quiet::Shader::Create(posVertexSrc, posFragmentSrc));
	}

	void OnUpdate(Quiet::Timestep ts) override
	{
		// Set debug Timestep 
		m_DebugTS = ts;

		// Camera Movement
		if (Quiet::Input::IsKeyPressed(Quiet::Key::Left))	
			m_CameraPosition.x -= m_CameraMovementSpeed * ts;
		else if (Quiet::Input::IsKeyPressed(Quiet::Key::Right))	
			m_CameraPosition.x += m_CameraMovementSpeed * ts;
		if (Quiet::Input::IsKeyPressed(Quiet::Key::Up))		
			m_CameraPosition.y += m_CameraMovementSpeed * ts;
		else if (Quiet::Input::IsKeyPressed(Quiet::Key::Down))	
			m_CameraPosition.y -= m_CameraMovementSpeed * ts;

		// Camera Rotation
		if (Quiet::Input::IsKeyPressed(Quiet::Key::Q))
			m_CameraRotation -= m_CameraRotationSpeed;
		if (Quiet::Input::IsKeyPressed(Quiet::Key::E))
			m_CameraRotation += m_CameraRotationSpeed;

		Quiet::RendererCommand::SetClearColor({ 0.1f, 0.1f, 0.1f, 1 });
		Quiet::RendererCommand::Clear();

		m_Camera.SetPosition(m_CameraPosition);
		m_Camera.SetRotation({ m_CameraRotation });

		Quiet::Renderer::BeginScene(m_Camera);

		glm::mat4 scale = glm::scale(glm::mat4(1.0f), glm::vec3(0.1f));

		std::dynamic_pointer_cast<Quiet::OpenGLShader>(m_FlatColorShader)->Bind();
		std::dynamic_pointer_cast<Quiet::OpenGLShader>(m_FlatColorShader)->UploadUniformFloat4("u_Color", m_Color);

		for (int y = -10; y < 10; y++)
		{
			for (int x = -10; x < 10; x++)
			{
				glm::vec3 pos(x * 0.11f, y * 0.11f, 0.0f);
				glm::mat4 transform = glm::translate(glm::mat4(1.0f), pos) * scale;
				Quiet::Renderer::Submit(m_FlatColorShader, m_SquareVA, transform);
			}

		}
		Quiet::Renderer::Submit(m_PositionShader, m_VertexArray);

		Quiet::Renderer::EndScene();
	}

	void OnImGuiRender() override
	{
		ImGui::Begin("Test");
		ImGui::TextColored({ 0.3f, 0.4f, 0.8f, 1.0f }, "Debug");
		ImGui::Text("Deltatime: %f s (%f ms)", m_DebugTS, m_DebugTS.GetMilliseconds());
		ImGui::Separator();

		ImGui::TextColored({ 0.3f, 0.4f, 0.8f, 1.0f }, "Camera Status");
		ImGui::Text("Position:");
		ImGui::Text("-> X: %f", m_CameraPosition.x);
		ImGui::Text("-> Y: %f", m_CameraPosition.y);
		ImGui::Text("Rotation:");
		ImGui::Text("-> %f",  m_CameraRotation);

		ImGui::Separator();
		ImGui::TextColored({ 0.3f, 0.4f, 0.8f, 1.0f }, "Camera Control");
		ImGui::SliderFloat("Rotation", &m_CameraRotation, -360, 360);
		ImGui::InputFloat("Movement Speed", &m_CameraMovementSpeed);
		ImGui::InputFloat("Rotation Speed", &m_CameraRotationSpeed);

		ImGui::Separator();
		ImGui::TextColored({ 0.3f, 0.4f, 0.8f, 1.0f }, "Box");
		ImGui::ColorEdit4("Square Color", glm::value_ptr(m_Color));
		ImGui::End();

	}

	void OnEvent(Quiet::Event& event) override
	{
		
	}

private:
	std::shared_ptr<Quiet::Shader> m_FlatColorShader;
	std::shared_ptr<Quiet::VertexArray> m_VertexArray;

	std::shared_ptr<Quiet::Shader> m_PositionShader;
	std::shared_ptr<Quiet::VertexArray> m_SquareVA;

	Quiet::CameraOrthographic m_Camera;
	glm::vec3 m_CameraPosition;
	float m_CameraRotation = 0;

	float m_CameraMovementSpeed = 5.0f;
	float m_CameraRotationSpeed = 0.5f;

	glm::vec4 m_Color = { 0.8f, 0.2f, 0.3f, 1.0f };

	// Debug
	Quiet::Timestep m_DebugTS;
};

class Sandbox : public Quiet::Application
{
public:
	Sandbox()
	{
		PushLayer(new TestLayer());
	}

	~Sandbox()
	{
		
	}
};

Quiet::Application* Quiet::CreateApplication()
{
	return new Sandbox();
}

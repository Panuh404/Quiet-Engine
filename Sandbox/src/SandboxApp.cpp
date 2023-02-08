#include <Quiet.h>

#include "ImGui/imgui.h"
#include "Quiet/Events/KeyEvent.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>


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
			-0.75f, -0.75f, 0.0f,
			 0.75f, -0.75f, 0.0f,
			 0.75f,  0.75f, 0.0f,
			-0.75f,  0.75f, 0.0f
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

		std::string vertexSrc = R"(
			#version 330 core
			
			layout(location = 0) in vec3 a_Position;
			layout(location = 1) in vec4 a_Color;
			uniform mat4 u_ViewProjection;
			out vec3 v_Position;
			out vec4 v_Color;
			void main()
			{
				v_Position = a_Position;
				v_Color = a_Color;
				gl_Position = u_ViewProjection * vec4(a_Position, 1.0);	
			}
		)";
		std::string fragmentSrc = R"(
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

		m_Shader.reset(new Quiet::Shader(vertexSrc, fragmentSrc));

		std::string blueShaderVertexSrc = R"(
			#version 330 core
			
			layout(location = 0) in vec3 a_Position;
			uniform mat4 u_ViewProjection;
			out vec3 v_Position;
			void main()
			{
				v_Position = a_Position;
				gl_Position = u_ViewProjection * vec4(a_Position, 1.0);	
			}
		)";

		std::string blueShaderFragmentSrc = R"(
			#version 330 core
			
			layout(location = 0) out vec4 color;
			in vec3 v_Position;
			void main()
			{
				color = vec4(0.2, 0.3, 0.8, 1.0);
			}
		)";

		m_BlueShader.reset(new Quiet::Shader(blueShaderVertexSrc, blueShaderFragmentSrc));
	}

	void OnUpdate(Quiet::Timestep ts) override
	{
		// Set debug Timestep 
		debugTS = ts;

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
		if (Quiet::Input::IsKeyPressed(Quiet::Key::A))
			m_CameraRotation -= m_CameraRotationSpeed;
		if (Quiet::Input::IsKeyPressed(Quiet::Key::D))
			m_CameraRotation += m_CameraRotationSpeed;


		Quiet::RendererCommand::SetClearColor({ 0.1f, 0.1f, 0.1f, 1 });
		Quiet::RendererCommand::Clear();

		m_Camera.SetPosition(m_CameraPosition);
		m_Camera.SetRotation({ m_CameraRotation });

		Quiet::Renderer::BeginScene(m_Camera);

		Quiet::Renderer::Submit(m_BlueShader, m_SquareVA);
		Quiet::Renderer::Submit(m_Shader, m_VertexArray);

		Quiet::Renderer::EndScene();
	}

	void OnImGuiRender() override
	{
		ImGui::Begin("Dialog");
		ImGui::TextColored({ 0.3f, 0.4f, 0.8f, 1.0f }, "Debug");
		ImGui::Text("Deltatime: %f s (%f ms)", debugTS, debugTS.GetMilliseconds());
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
		ImGui::End();

	}

	void OnEvent(Quiet::Event& event) override
	{
		Quiet::EventDispatcher dispatcher(event);
		dispatcher.Dispatch<Quiet::KeyPressedEvent>(BIND_EVENT_FN(TestLayer::OnKeyPressedEvent));
	}

	bool OnKeyPressedEvent(Quiet::KeyPressedEvent& event)
	{


		
		return false;
	}

private:
	std::shared_ptr<Quiet::Shader> m_Shader;
	std::shared_ptr<Quiet::VertexArray> m_VertexArray;

	std::shared_ptr<Quiet::Shader> m_BlueShader;
	std::shared_ptr<Quiet::VertexArray> m_SquareVA;

	Quiet::CameraOrthographic m_Camera;
	glm::vec3 m_CameraPosition;
	float m_CameraRotation = 0;

	float m_CameraMovementSpeed = 5.0f;
	float m_CameraRotationSpeed = 0.5f;

	// Debug
	Quiet::Timestep debugTS;
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

#pragma once

#include "Quiet/Core/Layer.h"

namespace Quiet
{
	class ImGuiLayer final : public Layer
	{
	public:
		ImGuiLayer();
		~ImGuiLayer() = default;

		void OnAttach() override;
		void OnDetach() override;
		void OnEvent(Event& e) override;

		void Begin();
		void End();

	private:
		float m_Time = 0.0f;
	};
}
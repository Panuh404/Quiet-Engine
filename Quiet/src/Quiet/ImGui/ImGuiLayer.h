#pragma once

#include "Quiet/Core/Layer.h"

#include "Quiet/Events/ApplicationEvent.h"
#include "Quiet/Events/KeyEvent.h"
#include "Quiet/Events/MouseEvent.h"

namespace Quiet
{
	class QUIET_API ImGuiLayer final : public Layer
	{
	public:
		ImGuiLayer();
		~ImGuiLayer();

		virtual void OnAttach() override;
		virtual void OnDetach() override;
		virtual void OnImGuiRender() override;

		void Begin();
		void End();

	private:
		float m_Time = 0.0f;
	};
}
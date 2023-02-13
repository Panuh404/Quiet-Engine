#pragma once

#include "Quiet/Events/Event.h"

#include <sstream>

namespace Quiet
{
	//-----------------------------------------------------------------------------
	// [EVENT] WindowResizeEvent
	//-----------------------------------------------------------------------------
	class QUIET_API WindowResizeEvent : public Event
	{
	public:
		WindowResizeEvent(uint32_t width, uint32_t height) :
			m_Width(width), m_Height(height) {}

		uint32_t GetWidth() const { return m_Width; }
		uint32_t GetHeight() const { return m_Height; }

		std::string ToString() const override
		{
			std::stringstream ss;
			ss << "WindowResizeEvent: " << m_Width << ", " << m_Height;
			return ss.str();
		}
		EVENT_CLASS_TYPE(WindowResize)
		EVENT_CLASS_CATEGORY(EventCategoryApplication)

	private:
		uint32_t m_Width, m_Height;
	};

	//-----------------------------------------------------------------------------
	// [EVENT] WindowCloseEvent
	//-----------------------------------------------------------------------------
	class QUIET_API WindowCloseEvent : public Event
	{
	public:
		WindowCloseEvent() = default;
		EVENT_CLASS_TYPE(WindowClose)
		EVENT_CLASS_CATEGORY(EventCategoryApplication)
	};

	//-----------------------------------------------------------------------------
	// [EVENT] AppTickEvent
	//-----------------------------------------------------------------------------
	class QUIET_API AppTickEvent : public Event
	{
	public:
		AppTickEvent() = default;
		EVENT_CLASS_TYPE(AppTick)
		EVENT_CLASS_CATEGORY(EventCategoryApplication)
	};

	//-----------------------------------------------------------------------------
	// [EVENT] AppUpdateEvent
	//-----------------------------------------------------------------------------
	class QUIET_API AppUpdateEvent : public Event
	{
	public:
		AppUpdateEvent() = default;
		EVENT_CLASS_TYPE(AppUpdate)
		EVENT_CLASS_CATEGORY(EventCategoryApplication)
	};

	//-----------------------------------------------------------------------------
	// [EVENT] AppRenderEvent
	//-----------------------------------------------------------------------------
	class QUIET_API AppRenderEvent : public Event
	{
	public:
		AppRenderEvent() = default;
		EVENT_CLASS_TYPE(AppRender)
		EVENT_CLASS_CATEGORY(EventCategoryApplication)
	};
}
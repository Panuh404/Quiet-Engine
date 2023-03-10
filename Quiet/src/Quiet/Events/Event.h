#pragma once

#include "Quiet/Core/Core.h"

#include <string>
#include <functional>

namespace Quiet
{
	// Events in Quiet Engine are currently blocking, meaning when an event occurs it 
	// immediately gets dispatched and must be dealt with right then an there.
	// For the future, a better strategy might be to buffer events in an event bus and
	// process them during the "event" part of the update stage.

	enum class EventType
	{
		None=0,
		WindowClose, WindowResize, WindowFocus, WindowLostFocus, WindowMoved,	// Window Events
		AppTick, AppUpdate, AppRender,											// Application Events
		KeyPressed, KeyReleased, KeyTyped,										// Key Events
		MouseButtonPressed, MouseButtonReleased, MouseMoved, MouseScrolled		// Mouse Events
	};

	enum EventCategory
	{
		None = 0,
		EventCategoryApplication	= BIT(0),
		EventCategoryInput			= BIT(1),
		EventCategoryKeyboard		= BIT(2),
		EventCategoryMouse			= BIT(3),
		EventCategoryMouseButton	= BIT(4)
	};

	#define EVENT_CLASS_TYPE(type)	static EventType GetStaticType() { return EventType::type; }\
									virtual EventType GetEventType() const override { return GetStaticType(); }\
									virtual const char* GetName() const override { return #type; }

	#define EVENT_CLASS_CATEGORY(category) virtual int GetCategoryFlags() const override { return category; }

	//-----------------------------------------------------------------------------
	// [CLASS] Event Base
	//-----------------------------------------------------------------------------
	class Event
	{
		friend class EventDispatcher;
	public:
		bool Handled = false;

		virtual EventType GetEventType() const = 0;
		virtual const char* GetName() const = 0;
		virtual int GetCategoryFlags() const = 0;
		virtual std::string ToString() const { return GetName(); }

		bool IsInCategory(EventCategory category) { return GetCategoryFlags() & category; }
	};

	//-----------------------------------------------------------------------------
	// [CLASS] Event Dispatcher
	//-----------------------------------------------------------------------------
	class EventDispatcher
	{
		template<typename T>
		using EventFn = std::function<bool(T&)>;
	public:
		EventDispatcher(Event& event) : m_Event(event) {}

		template<typename T>
		bool Dispatch(EventFn<T> func)
		{
			if (m_Event.GetEventType() == T::GetStaticType())
			{
				m_Event.Handled = func(*(T*)&m_Event);
				return true;
			}
			return false;
		}
	private:
		Event& m_Event;
	};

	inline std::ostream& operator<<(std::ostream& os, const Event& e)
	{
		return os << e.ToString();
	}
}
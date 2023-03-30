#pragma once

#include "Quiet/Events/Event.h"

namespace Quiet
{
	//-----------------------------------------------------------------------------
	// [EVENT] KeyEvent
	//-----------------------------------------------------------------------------
	class KeyEvent : public Event
	{
	public:
		int GetKeyCode() const { return m_KeyCode; }
		EVENT_CLASS_CATEGORY(EventCategoryKeyboard | EventCategoryInput)

	protected:
		KeyEvent(int keycode) : m_KeyCode(keycode) {}
		int m_KeyCode;
	};

	//-----------------------------------------------------------------------------
	// [KeyEvent] KeyPressedEvent
	//-----------------------------------------------------------------------------
	class KeyPressedEvent : public KeyEvent
	{
	public:
		KeyPressedEvent(int keycode, int repeatCount) : KeyEvent(keycode), m_RepeatCount(repeatCount) {}
		int GetRepeatCount() const { return m_RepeatCount; }

		std::string ToString() const override
		{
			std::stringstream ss;
			ss << "KeyPressedEvent: " << m_KeyCode << " (" << m_RepeatCount << " repeats)";
			return ss.str();
		}
		EVENT_CLASS_TYPE(KeyPressed)

	private:
		int m_RepeatCount;
	};

	//-----------------------------------------------------------------------------
	// [KeyEvent] KeyReleasedEvent
	//-----------------------------------------------------------------------------
	class KeyReleasedEvent : public KeyEvent
	{
	public:
		KeyReleasedEvent(int key) : KeyEvent(key) {}

		std::string ToString() const override
		{
			std::stringstream ss;
			ss << "KeyReleasedEvent: " << m_KeyCode;
			return ss.str();
		}
		EVENT_CLASS_TYPE(KeyReleased)
	};

	//-----------------------------------------------------------------------------
	// [KeyEvent] KeyTypedEvent
	//-----------------------------------------------------------------------------
	class KeyTypedEvent : public KeyEvent
	{
	public:
		KeyTypedEvent(int key) : KeyEvent(key) {}

		std::string ToString() const override
		{
			std::stringstream ss;
			ss << "KeyTypedEvent: " << m_KeyCode;
			return ss.str();
		}
		EVENT_CLASS_TYPE(KeyTyped)
	};
}
#include "Scripting/Bindings/Bindings.h"

#include <cstdint>
#include <tuple>

#include "Quiet/Input/Input.h"
#include "Quiet/Input/KeyCodes.h"
#include "Quiet/Input/MouseCodes.h"

namespace
{
    constexpr std::int32_t ToValue(
        const Quiet::Input::KeyCode key)
    {
        return static_cast<std::int32_t>(key);
    }

    constexpr std::int32_t ToValue(
        const Quiet::Input::MouseCode button)
    {
        return static_cast<std::int32_t>(button);
    }

    constexpr bool IsBetween(
        const std::int32_t value,
        const std::int32_t minimum,
        const std::int32_t maximum)
    {
        return
            value >= minimum &&
            value <= maximum;
    }

    bool IsSupportedKeyCode(
        const std::int32_t value)
    {
        using Quiet::Input::KeyCode;

        return
            value == ToValue(KeyCode::Space) ||
            value == ToValue(KeyCode::Apostrophe) ||

            IsBetween(
                value,
                ToValue(KeyCode::Comma),
                ToValue(KeyCode::D9)) ||

            value == ToValue(KeyCode::Semicolon) ||
            value == ToValue(KeyCode::Equal) ||

            IsBetween(
                value,
                ToValue(KeyCode::A),
                ToValue(KeyCode::RightBracket)) ||

            value == ToValue(KeyCode::GraveAccent) ||

            IsBetween(
                value,
                ToValue(KeyCode::World1),
                ToValue(KeyCode::World2)) ||

            IsBetween(
                value,
                ToValue(KeyCode::Escape),
                ToValue(KeyCode::End)) ||

            IsBetween(
                value,
                ToValue(KeyCode::CapsLock),
                ToValue(KeyCode::Pause)) ||

            IsBetween(
                value,
                ToValue(KeyCode::F1),
                ToValue(KeyCode::F25)) ||

            IsBetween(
                value,
                ToValue(KeyCode::KP0),
                ToValue(KeyCode::KPEqual)) ||

            IsBetween(
                value,
                ToValue(KeyCode::LeftShift),
                ToValue(KeyCode::Menu));
    }

    bool IsSupportedMouseCode(
        const std::int32_t value)
    {
        return IsBetween(
            value,
            ToValue(Quiet::Input::MouseCode::Button0),
            ToValue(Quiet::Input::MouseCode::ButtonLast));
    }
}

namespace Quiet::Scripting::Bindings
{
    void InstallInputBindings(sol::state& lua, sol::environment& environment, const Input::Input& input)
    {
        sol::table keyTable = lua.create_table();

        const auto addKey = [&keyTable](const char* name, const Input::KeyCode key)
        {
            keyTable[name] = ToValue(key);
        };

        addKey("Space", Input::KeyCode::Space);
        addKey("Apostrophe", Input::KeyCode::Apostrophe);
        addKey("Comma", Input::KeyCode::Comma);
        addKey("Minus", Input::KeyCode::Minus);
        addKey("Period", Input::KeyCode::Period);
        addKey("Slash", Input::KeyCode::Slash);

        addKey("D0", Input::KeyCode::D0);
        addKey("D1", Input::KeyCode::D1);
        addKey("D2", Input::KeyCode::D2);
        addKey("D3", Input::KeyCode::D3);
        addKey("D4", Input::KeyCode::D4);
        addKey("D5", Input::KeyCode::D5);
        addKey("D6", Input::KeyCode::D6);
        addKey("D7", Input::KeyCode::D7);
        addKey("D8", Input::KeyCode::D8);
        addKey("D9", Input::KeyCode::D9);

        addKey("Semicolon", Input::KeyCode::Semicolon);
        addKey("Equal", Input::KeyCode::Equal);

        addKey("A", Input::KeyCode::A);
        addKey("B", Input::KeyCode::B);
        addKey("C", Input::KeyCode::C);
        addKey("D", Input::KeyCode::D);
        addKey("E", Input::KeyCode::E);
        addKey("F", Input::KeyCode::F);
        addKey("G", Input::KeyCode::G);
        addKey("H", Input::KeyCode::H);
        addKey("I", Input::KeyCode::I);
        addKey("J", Input::KeyCode::J);
        addKey("K", Input::KeyCode::K);
        addKey("L", Input::KeyCode::L);
        addKey("M", Input::KeyCode::M);
        addKey("N", Input::KeyCode::N);
        addKey("O", Input::KeyCode::O);
        addKey("P", Input::KeyCode::P);
        addKey("Q", Input::KeyCode::Q);
        addKey("R", Input::KeyCode::R);
        addKey("S", Input::KeyCode::S);
        addKey("T", Input::KeyCode::T);
        addKey("U", Input::KeyCode::U);
        addKey("V", Input::KeyCode::V);
        addKey("W", Input::KeyCode::W);
        addKey("X", Input::KeyCode::X);
        addKey("Y", Input::KeyCode::Y);
        addKey("Z", Input::KeyCode::Z);

        addKey("LeftBracket", Input::KeyCode::LeftBracket);
        addKey("Backslash", Input::KeyCode::Backslash);
        addKey("RightBracket", Input::KeyCode::RightBracket);
        addKey("GraveAccent", Input::KeyCode::GraveAccent);

        addKey("World1", Input::KeyCode::World1);
        addKey("World2", Input::KeyCode::World2);

        addKey("Escape", Input::KeyCode::Escape);
        addKey("Enter", Input::KeyCode::Enter);
        addKey("Tab", Input::KeyCode::Tab);
        addKey("Backspace", Input::KeyCode::Backspace);
        addKey("Insert", Input::KeyCode::Insert);
        addKey("Delete", Input::KeyCode::Delete);
        addKey("Right", Input::KeyCode::Right);
        addKey("Left", Input::KeyCode::Left);
        addKey("Down", Input::KeyCode::Down);
        addKey("Up", Input::KeyCode::Up);
        addKey("PageUp", Input::KeyCode::PageUp);
        addKey("PageDown", Input::KeyCode::PageDown);
        addKey("Home", Input::KeyCode::Home);
        addKey("End", Input::KeyCode::End);
        addKey("CapsLock", Input::KeyCode::CapsLock);
        addKey("ScrollLock", Input::KeyCode::ScrollLock);
        addKey("NumLock", Input::KeyCode::NumLock);
        addKey("PrintScreen", Input::KeyCode::PrintScreen);
        addKey("Pause", Input::KeyCode::Pause);

        addKey("F1", Input::KeyCode::F1);
        addKey("F2", Input::KeyCode::F2);
        addKey("F3", Input::KeyCode::F3);
        addKey("F4", Input::KeyCode::F4);
        addKey("F5", Input::KeyCode::F5);
        addKey("F6", Input::KeyCode::F6);
        addKey("F7", Input::KeyCode::F7);
        addKey("F8", Input::KeyCode::F8);
        addKey("F9", Input::KeyCode::F9);
        addKey("F10", Input::KeyCode::F10);
        addKey("F11", Input::KeyCode::F11);
        addKey("F12", Input::KeyCode::F12);
        addKey("F13", Input::KeyCode::F13);
        addKey("F14", Input::KeyCode::F14);
        addKey("F15", Input::KeyCode::F15);
        addKey("F16", Input::KeyCode::F16);
        addKey("F17", Input::KeyCode::F17);
        addKey("F18", Input::KeyCode::F18);
        addKey("F19", Input::KeyCode::F19);
        addKey("F20", Input::KeyCode::F20);
        addKey("F21", Input::KeyCode::F21);
        addKey("F22", Input::KeyCode::F22);
        addKey("F23", Input::KeyCode::F23);
        addKey("F24", Input::KeyCode::F24);
        addKey("F25", Input::KeyCode::F25);

        addKey("KP0", Input::KeyCode::KP0);
        addKey("KP1", Input::KeyCode::KP1);
        addKey("KP2", Input::KeyCode::KP2);
        addKey("KP3", Input::KeyCode::KP3);
        addKey("KP4", Input::KeyCode::KP4);
        addKey("KP5", Input::KeyCode::KP5);
        addKey("KP6", Input::KeyCode::KP6);
        addKey("KP7", Input::KeyCode::KP7);
        addKey("KP8", Input::KeyCode::KP8);
        addKey("KP9", Input::KeyCode::KP9);

        addKey("KPDecimal", Input::KeyCode::KPDecimal);
        addKey("KPDivide", Input::KeyCode::KPDivide);
        addKey("KPMultiply", Input::KeyCode::KPMultiply);
        addKey("KPSubtract", Input::KeyCode::KPSubtract);
        addKey("KPAdd", Input::KeyCode::KPAdd);
        addKey("KPEnter", Input::KeyCode::KPEnter);
        addKey("KPEqual", Input::KeyCode::KPEqual);

        addKey("LeftShift", Input::KeyCode::LeftShift);
        addKey("LeftControl", Input::KeyCode::LeftControl);
        addKey("LeftAlt", Input::KeyCode::LeftAlt);
        addKey("LeftSuper", Input::KeyCode::LeftSuper);
        addKey("RightShift", Input::KeyCode::RightShift);
        addKey("RightControl", Input::KeyCode::RightControl);
        addKey("RightAlt", Input::KeyCode::RightAlt);
        addKey("RightSuper", Input::KeyCode::RightSuper);
        addKey("Menu", Input::KeyCode::Menu);

        sol::table mouseButtonTable = lua.create_table();

        const auto addMouseButton = [&mouseButtonTable](const char* name, const Input::MouseCode button)
        {
            mouseButtonTable[name] = ToValue(button);
        };

        addMouseButton("Button0", Input::MouseCode::Button0);
        addMouseButton("Button1", Input::MouseCode::Button1);
        addMouseButton("Button2", Input::MouseCode::Button2);
        addMouseButton("Button3", Input::MouseCode::Button3);
        addMouseButton("Button4", Input::MouseCode::Button4);
        addMouseButton("Button5", Input::MouseCode::Button5);
        addMouseButton("Button6", Input::MouseCode::Button6);
        addMouseButton("Button7", Input::MouseCode::Button7);

        addMouseButton("Left", Input::MouseCode::ButtonLeft);
        addMouseButton("Right", Input::MouseCode::ButtonRight);
        addMouseButton("Middle", Input::MouseCode::ButtonMiddle);

        sol::table keyboardTable = lua.create_table();

        keyboardTable.set_function("IsDown", [&input](const std::int32_t rawKey)
        {
            if (!IsSupportedKeyCode(rawKey)) return false;
            return input.IsKeyDown(static_cast<Input::KeyCode>(rawKey));
        });

        keyboardTable.set_function("IsPressed", [&input](const std::int32_t rawKey)
        {
            if (!IsSupportedKeyCode(rawKey)) return false;
            return input.IsKeyPressed(static_cast<Input::KeyCode>(rawKey));
        });

        sol::table mouseTable = lua.create_table();

        mouseTable.set_function("IsDown", [&input](const std::int32_t rawButton)
        {
            if (!IsSupportedMouseCode(rawButton)) return false;
            return input.IsMouseButtonDown(static_cast<Input::MouseCode>(rawButton));
        });

        mouseTable.set_function("IsPressed", [&input](const std::int32_t rawButton)
        {
            if (!IsSupportedMouseCode(rawButton)) return false;
            return input.IsMouseButtonPressed(static_cast<Input::MouseCode>(rawButton));
        });

        mouseTable.set_function("GetDelta", [&input]()
        {
            const auto [x, y] = input.GetMouseDelta();
            return std::make_tuple(x, y);
        });

        sol::table inputTable = lua.create_table();

        inputTable["Keyboard"] = keyboardTable;
        inputTable["Mouse"] = mouseTable;
        environment["Input"] = inputTable;
        environment["Key"] = keyTable;
        environment["MouseButton"] = mouseButtonTable;
    }
}
#pragma once
#include <Windows.h>
#include <Mouse.h>
#include <Keyboard.h>
#include <memory>
#include <DirectXMath.h>

class InputManager
{
public:
	static void Init(HWND windowHandle);
	static void Update();

	static void MouseReset();
	static const DirectX::Mouse::ButtonStateTracker& GetMouse();
	static const DirectX::Keyboard::KeyboardStateTracker& GetKeyboard();

private:
	static InputManager s_instance;
	std::unique_ptr<DirectX::Mouse>		m_mouse;
	std::unique_ptr<DirectX::Keyboard>	m_keyboard;
	DirectX::Mouse::ButtonStateTracker	m_mouseTracker;
	DirectX::Keyboard::KeyboardStateTracker m_keyboardTracker;
};
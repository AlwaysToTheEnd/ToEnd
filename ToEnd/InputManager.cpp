#include "InputManager.h"

InputManager InputManager::s_instance;

void InputManager::Init(HWND windowHandle)
{
	s_instance.m_mouse = std::make_unique<DirectX::Mouse>();
	s_instance.m_keyboard = std::make_unique<DirectX::Keyboard>();
	s_instance.m_keyboard->Reset();
	s_instance.m_mouse->SetWindow(windowHandle);
	s_instance.m_mouse->ResetScrollWheelValue();

	s_instance.m_keyboardTracker.Reset();
	s_instance.m_mouseTracker.Reset();
}

void InputManager::Update()
{
	s_instance.m_keyboardTracker.Update(s_instance.m_keyboard->GetState());
	s_instance.m_mouseTracker.Update(s_instance.m_mouse->GetState());
	s_instance.m_mouse->ResetScrollWheelValue();
}

void InputManager::MouseReset()
{
	s_instance.m_mouseTracker.Reset();
}

const DirectX::Mouse::ButtonStateTracker& InputManager::GetMouse()
{
	return s_instance.m_mouseTracker;
}

const DirectX::Keyboard::KeyboardStateTracker& InputManager::GetKeyboard()
{
	return s_instance.m_keyboardTracker;
}

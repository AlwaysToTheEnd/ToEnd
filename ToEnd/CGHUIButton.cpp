#include "CGHUIButton.h"
#include "InputManager.h"

CGHUIButton::CGHUIButton()
{
}

CGHUIButton::CGHUIButton(const CGHUIButton& rhs)
{
	assert(false);
}

CGHUIButton::~CGHUIButton()
{

}

void CGHUIButton::Init()
{
	m_uiTransform = CreateComponent<COMUITransform>();
	m_backRender = CreateComponent<COMUIRenderer>();
}

void CGHUIButton::SetPos(unsigned int x, unsigned int y, float z)
{
	m_uiTransform->SetPos(DirectX::XMVectorSet(x,y,z,0));
}

void CGHUIButton::SetSize(unsigned int x, unsigned int y)
{
	m_uiTransform->SetSize(DirectX::XMVectorSet(x, y, 0, 0));
}

void XM_CALLCONV CGHUIButton::SetColor(DirectX::FXMVECTOR color)
{
	m_backRender->SetColor(color);
}

void CGHUIButton::AddFunc(int mousebutton, int mouseState, std::function<void(CGHNode*)> func)
{
	m_backRender->AddFunc(mousebutton, mouseState, func);
}


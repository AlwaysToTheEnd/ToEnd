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

	m_childFontNode.SetParent(this, true);
	m_fontTransform = m_childFontNode.CreateComponent<COMUITransform>();
	m_fontRenderer = m_childFontNode.CreateComponent<COMFontRenderer>();
	m_fontRenderer->SetParentRender(m_backRender);
	m_fontTransform->SetPos(DirectX::XMVectorSet(0, 0, -0.010f, 0));
}

void CGHUIButton::SetPos(unsigned int x, unsigned int y, float z)
{
	m_uiTransform->SetPos(DirectX::XMVectorSet(x,y,z,0));
}

void CGHUIButton::SetSize(unsigned int x, unsigned int y)
{
	m_uiTransform->SetSize(DirectX::XMVectorSet(x, y, 0, 0));
	m_fontRenderer->SetFontSize(y);
	m_fontRenderer->SetRowPitch(x * 0.95f);
}

void CGHUIButton::SetText(const wchar_t* text)
{
	m_fontRenderer->SetText(text);
}

void XM_CALLCONV CGHUIButton::SetColor(DirectX::FXMVECTOR color)
{
	m_backRender->SetColor(color);
}

void XM_CALLCONV CGHUIButton::SetFontColor(DirectX::FXMVECTOR color)
{
	m_fontRenderer->SetColor(color);
}

void CGHUIButton::AddFunc(int mousebutton, int mouseState, std::function<void(CGHNode*)> func)
{
	m_backRender->AddFunc(mousebutton, mouseState, this, func);
}

void CGHUIButton::SetFunc(int mousebutton, int mouseState, std::function<void(CGHNode*)> func)
{
	m_backRender->SetFunc(mousebutton, mouseState, this, func);
}


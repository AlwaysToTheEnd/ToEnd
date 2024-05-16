#include "CGHUIPanel.h"
#include "InputManager.h"

unsigned int CGHUIPanel::s_currButtonIndex = 0;
unsigned int CGHUIPanel::s_prevButtonIndex = 0;
unsigned int CGHUIPanel::s_currFontIndex = 0;
unsigned int CGHUIPanel::s_prevFontIndex = 0;
CGHUIButton CGHUIPanel::s_buttons[maxNumButton] = {};
CGHNode CGHUIPanel::s_fonts[maxNumButton] = {};

CGHUIPanel::CGHUIPanel()
{
}

CGHUIPanel::CGHUIPanel(const CGHUIPanel& rhs)
{
	assert(false);
}

CGHUIPanel::~CGHUIPanel()
{

}

void CGHUIPanel::Init()
{
	m_uiTransform = CreateComponent<COMUITransform>();
	m_backRender = CreateComponent<COMUIRenderer>();
}

void CGHUIPanel::Update(float delta)
{
	CGHNode::Update(delta);
}

void CGHUIPanel::RateUpdate(float delta)
{
	for (unsigned int i = s_prevButtonIndex; i < s_currButtonIndex; ++i)
	{
		s_buttons[i].SetActive(false);
	}

	for (unsigned int i = s_prevFontIndex; i < s_currFontIndex; ++i)
	{
		s_fonts[i].SetActive(false);
	}

	CGHNode::RateUpdate(delta);
}

void CGHUIPanel::SetSize(unsigned int x, unsigned int y)
{
	m_uiTransform->SetSize(DirectX::XMVectorSet(x, y, 0, 0));
}

void CGHUIPanel::SetPos(unsigned int x, unsigned int y, float z)
{
	m_uiTransform->SetPos(DirectX::XMVectorSet(x, y, z, 0));
}

void CGHUIPanel::BeginHorizontal()
{
	m_isHorizontal = true;
}

void CGHUIPanel::BeginVertical()
{
	m_isVertical = true;
}

void CGHUIPanel::ActiveScroll(bool active, bool isHorizontal)
{
	if (isHorizontal)
	{
		m_activeHorizontalScroll = active;
	}
	else
	{
		m_activeVerticalScroll = active;
	}
}

void CGHUIPanel::EndHorizontal()
{
	m_isHorizontal = false;
}

void CGHUIPanel::EndVertical()
{
	m_isVertical = false;
}

void CGHUIPanel::Button(unsigned int width, unsigned int hegith, std::function<void(CGHNode*)> func, int mousebutton, int mouseState)
{
	
}

void CGHUIPanel::Font(const char* text, unsigned int fontSize, unsigned int rowPitch, DirectX::FXMVECTOR color)
{
}

void CGHUIPanel::InitUIPanelSys()
{
	for (auto& iter : s_buttons)
	{
		iter.Init();
	}

	for (auto& iter : s_fonts)
	{
		iter.CreateComponent<COMUITransform>();
		iter.CreateComponent<COMFontRenderer>();
	}
}

void CGHUIPanel::ResetButtonIndex()
{
	s_prevButtonIndex = s_currButtonIndex;
	s_currButtonIndex = 0;
	s_prevFontIndex = s_currFontIndex;
	s_currFontIndex = 0;
}

unsigned int CGHUIPanel::GetButtonindex()
{
	assert(s_currButtonIndex + 1 < maxNumButton);

	return 0;
}

unsigned int CGHUIPanel::GetFontindex()
{
	return 0;
}


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
	auto panelSize = m_uiTransform->GetSize();
	
	for (auto& iter : m_layoutStack)
	{
		switch (iter.type)
		{
		case UILAYOUT::HORIZONTAL:
		{

		}
		break;
		case UILAYOUT::HORIZONTALEND:
		{
		}
		break;
		case UILAYOUT::VERTICAL:
		{
		}
		break;
		case UILAYOUT::VERTICALEND:
		{
		}
		break;
		}
	}

	for (auto& iter : m_layoutStack)
	{
		switch (iter.type)
		{
		case UILAYOUT::HORIZONTAL:
		{

		}
		break;
		case UILAYOUT::HORIZONTALEND:
		{
		}
		break;
		case UILAYOUT::VERTICAL:
		{
		}
		break;
		case UILAYOUT::VERTICALEND:
		{
		}
		break;
		case UILAYOUT::BUTTON:
		{
		}
		break;
		case UILAYOUT::FONT:
		{
		}
		break;
		}
	}

	CGHNode::Update(delta);

	m_layoutStack.clear();
}

void CGHUIPanel::RateUpdate(float delta)
{
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

void CGHUIPanel::BeginHorizontal(bool activeScroll)
{
	m_layoutStack.emplace_back();
	auto& layout = m_layoutStack.back();
	layout.type = UILAYOUT::HORIZONTAL;
	layout.isScroll = activeScroll;
}

void CGHUIPanel::EndHorizontal()
{
	m_layoutStack.emplace_back();
	auto& layout = m_layoutStack.back();
	layout.type = UILAYOUT::HORIZONTALEND;
}

void CGHUIPanel::BeginVertical(bool activeScroll)
{
	m_layoutStack.emplace_back();
	auto& layout = m_layoutStack.back();
	layout.type = UILAYOUT::VERTICAL;
	layout.isScroll = activeScroll;
}

void CGHUIPanel::EndVertical()
{
	m_layoutStack.emplace_back();
	auto& layout = m_layoutStack.back();
	layout.type = UILAYOUT::VERTICALEND;
}

void CGHUIPanel::Button(unsigned int width, unsigned int hegith, std::function<void(CGHNode*)> func, int mousebutton, int mouseState)
{
	m_layoutStack.emplace_back();
	auto& layout = m_layoutStack.back();
	layout.type = UILAYOUT::BUTTON;
	layout.buttonIndex = s_currButtonIndex;
	s_buttons[s_currButtonIndex].SetActive(true);
	s_buttons[s_currButtonIndex].SetParent(this, true);
	s_buttons[s_currButtonIndex].SetFunc(mousebutton, mouseState, func);
	s_buttons[s_currButtonIndex].SetSize(width, hegith);

	s_currButtonIndex++;
}

void CGHUIPanel::Font(const wchar_t* text, unsigned int fontSize, unsigned int rowPitch, DirectX::FXMVECTOR color)
{
	m_layoutStack.emplace_back();
	auto& layout = m_layoutStack.back();
	layout.type = UILAYOUT::FONT;
	layout.fontIndex = s_currFontIndex;

	s_fonts[s_currFontIndex].SetActive(true);
	s_fonts[s_currFontIndex].SetParent(this, true);
	auto fontRender = s_fonts[s_currFontIndex].GetComponent<COMFontRenderer>();
	fontRender->SetRenderString(text, color, rowPitch);
	fontRender->SetFontSize(fontSize);

	s_currFontIndex++;
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
	for (unsigned int i = s_prevButtonIndex; i < s_currButtonIndex; ++i)
	{
		s_buttons[i].SetActive(false);
	}

	for (unsigned int i = s_prevFontIndex; i < s_currFontIndex; ++i)
	{
		s_fonts[i].SetActive(false);
	}

	s_prevButtonIndex = s_currButtonIndex;
	s_currButtonIndex = 0;
	s_prevFontIndex = s_currFontIndex;
	s_currFontIndex = 0;
}


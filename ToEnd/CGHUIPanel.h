#pragma once
#include "CGHBaseClass.h"
#include "CGHUIButton.h"

class CGHUIPanel : public CGHNode
{
	enum UILAYOUT
	{
		HORIZONTAL,
		HORIZONTALEND,
		VERTICAL,
		VERTICALEND,
		BUTTON,
		FONT,
	};

	struct UILayoutDesc
	{
		UILAYOUT type;
		union
		{
			unsigned int buttonIndex;
			unsigned int fontIndex;
			bool isScroll;
		};
	};

	static const size_t maxNumButton = 512;
public:
	CGHUIPanel();
	CGHUIPanel(const CGHUIPanel& rhs);
	virtual ~CGHUIPanel();

	virtual void Init() override;
	virtual void Update(float delta) override;
	virtual void RateUpdate(float delta) override;

	void SetSize(unsigned int x, unsigned int y);
	void SetPos(unsigned int x, unsigned int y, float z);

	void BeginHorizontal(bool activeScroll);
	void BeginVertical(bool activeScroll);
	void EndHorizontal();
	void EndVertical();

	void Button(unsigned int width, unsigned int hegith, std::function<void(CGHNode*)> func, int mousebutton =0, int mouseState = 2);
	void Font(const wchar_t* text, unsigned int fontSize, unsigned int rowPitch, DirectX::FXMVECTOR color);

	static void InitUIPanelSys();
	static void ResetButtonIndex();

private:
	static unsigned int s_currButtonIndex;
	static unsigned int s_prevButtonIndex;
	static unsigned int s_currFontIndex;
	static unsigned int s_prevFontIndex;
	static CGHUIButton s_buttons[maxNumButton];
	static CGHNode s_fonts[maxNumButton];

	COMUITransform* m_uiTransform = nullptr;
	COMUIRenderer* m_backRender = nullptr;
	DirectX::XMFLOAT2 m_scrollPos = { 0,0 };
	
	std::vector<UILayoutDesc> m_layoutStack;
};


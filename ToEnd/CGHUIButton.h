#pragma once
#include "CGHBaseClass.h"

class CGHUIButton : public CGHNode
{
public:
	CGHUIButton();
	CGHUIButton(const CGHUIButton& rhs);
	virtual ~CGHUIButton();

	virtual void Init() override;

	void SetPos(unsigned int x, unsigned int y, float z);
	void SetSize(unsigned int x, unsigned int y);
	void SetText(const wchar_t* text);
	void XM_CALLCONV SetColor(DirectX::FXMVECTOR color);
	void XM_CALLCONV SetFontColor(DirectX::FXMVECTOR color);
	void AddFunc(int mousebutton, int mouseState, std::function<void(CGHNode*)> func);
	void SetFunc(int mousebutton, int mouseState, std::function<void(CGHNode*)> func);

private:
	CGHNode m_childFontNode;
	COMUITransform* m_uiTransform = nullptr;
	COMUIRenderer* m_backRender = nullptr;

	COMUITransform* m_fontTransform = nullptr;
	COMFontRenderer* m_fontRenderer = nullptr;
};


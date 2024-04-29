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
	void XM_CALLCONV SetColor(DirectX::FXMVECTOR color);
	void AddFunc(int mousebutton, int mouseState, std::function<void(CGHNode*)> func);

private:
	COMUITransform* m_uiTransform = nullptr;
	COMUIRenderer* m_backRender = nullptr;
};


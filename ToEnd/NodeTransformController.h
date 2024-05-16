#pragma once
#include <vector>
#include "CGHBaseClass.h"
#include "CGHUIButton.h"

class NodeTransformController : public CGHNode
{
	static const size_t maxNumButton = 256;
public:
	NodeTransformController();
	virtual ~NodeTransformController();

	virtual void Init() override;
	virtual void Update(float delta) override;
	virtual void RateUpdate(float delta) override;
	void SetSize(unsigned int x, unsigned int y);
	void SetPos(unsigned int x, unsigned int y, float z);

private:
	std::vector<CGHNode*> m_currNodeTree;
	std::vector<COMTransform*> m_currTransforms;
	CGHNode* m_prevTarget = nullptr;
	COMUITransform* m_uiTransform = nullptr;
	COMUIRenderer* m_backRender = nullptr;
	unsigned int m_currButtonIndex = 0;
};


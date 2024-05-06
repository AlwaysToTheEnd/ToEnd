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

private:
	CGHUIButton m_buttonPool[maxNumButton];
};


#pragma once
#include "CGHBaseClass.h"

class NodeTransformController : public CGHNode
{
public:
	NodeTransformController();
	virtual ~NodeTransformController();

	virtual void Init() override;
	virtual void Update(unsigned int currFrame, float delta) override;
	virtual void RateUpdate(unsigned int currFrame, float delta) override;

private:
	
};


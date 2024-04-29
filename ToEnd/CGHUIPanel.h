#pragma once
#include "CGHBaseClass.h"
class CGHUIPanel :
    public CGHNode
{
public:
	CGHUIPanel();
	CGHUIPanel(const CGHUIPanel& rhs);
	virtual ~CGHUIPanel();

	virtual void Init() override;
	virtual void Update(unsigned int currFrame, float delta) override;
	virtual void RateUpdate(unsigned int currFrame, float delta) override;

private:

};


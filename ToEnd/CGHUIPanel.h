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

private:

};


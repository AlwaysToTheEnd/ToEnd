#include "CGHUIPanel.h"
#include "InputManager.h"


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

}

void CGHUIPanel::Update(unsigned int currFrame, float delta)
{
	CGHNode::Update(currFrame, delta);
}

void CGHUIPanel::RateUpdate(unsigned int currFrame, float delta)
{
	CGHNode::RateUpdate(currFrame, delta);
}

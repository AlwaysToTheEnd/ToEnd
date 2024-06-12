#pragma once
#include <vector>
#include "CGHBaseClass.h"

class NodeTransformController : public CGHNode
{
public:
	NodeTransformController();
	virtual ~NodeTransformController();

	virtual void Update(float delta) override;
	virtual void RenderGUI(unsigned int currFrame) override;

private:
	void RenderNodeTransform(CGHNode* node, unsigned int uid);

private:
	CGHNode* m_currTarget = nullptr;
	CGHNode* m_currSelected = nullptr;
	DirectX::XMFLOAT3 m_rotation = {};
};


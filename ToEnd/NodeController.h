#pragma once
#include <vector>
#include "CGHBaseClass.h"

class NodeController : public CGHNode
{
public:
	NodeController();
	virtual ~NodeController();

	virtual void Update(float delta) override;
	virtual void RenderGUI(unsigned int currFrame) override;

private:
	void RenderNodeTree(CGHNode* node, unsigned int uid);

private:
	CGHNode* m_currTarget = nullptr;
	CGHNode* m_currSelected = nullptr;
	DirectX::XMFLOAT3 m_rotate = {};
};


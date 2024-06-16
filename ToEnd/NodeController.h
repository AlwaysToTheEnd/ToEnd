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
	void RenderRootNodes(std::vector<CGHNode*>& rootNodes);

private:
	void RenderNodeTree(CGHNode* node, void* uid);
	void SelectedNodeRemoved(CGHNode* node);

private:
	std::vector<CGHNode*> m_rootNodeList;
	CGHNode* m_currSelected = nullptr;
	DirectX::XMFLOAT3 m_rotate = {};
};


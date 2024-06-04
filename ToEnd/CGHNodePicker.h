#pragma once
#include "CGHBaseClass.h"

class CGHNodePicker
{
public:
	static CGHNodePicker s_instance;

public:
	void Init();
	CGHNode* GetCurrPickedNode(CGHNODE_LAYER layer) { return m_currPickedNode[static_cast<unsigned int>(layer)]; }

private:
	CGHNodePicker() = default;
	~CGHNodePicker() = default;

	void PickNode(CGHNode* node);
	void TargetNodeDeleted(CGHNODE_LAYER layer);

private:
	CGHNode* m_currPickedNode[static_cast<unsigned int>(CGHNODE_LAYER::CGHNODE_LYAER_NUM)];
};


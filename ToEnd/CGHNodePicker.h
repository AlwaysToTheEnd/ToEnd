#pragma once
#include "CGHBaseClass.h"

class CGHNodePicker
{
public:
	static CGHNodePicker s_instance;

public:
	void Init();
	CGHNode* GetCurrPickedNode() { return m_currPickedNode; }

private:
	CGHNodePicker() = default;
	~CGHNodePicker() = default;

	void PickNode(CGHNode* node);
	void TargetNodeDeleted();

private:
	CGHNode* m_currPickedNode;
};


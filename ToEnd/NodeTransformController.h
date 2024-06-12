#pragma once
#include <vector>
#include "CGHBaseClass.h"

class NodeTransformController : public CGHNode
{
	static const size_t maxNumButton = 256;
public:
	NodeTransformController();
	virtual ~NodeTransformController();

	virtual void Init() override;
	virtual void Update(float delta) override;
	virtual void RateUpdate(float delta) override;
	virtual void RenderGUI(unsigned int currFrame) override;
	void SetSize(unsigned int x, unsigned int y);
	void SetPos(unsigned int x, unsigned int y, float z);

private:
	void RenderNodeTransform(CGHNode* node, unsigned int uid);

private:
	CGHNode* m_currTarget = nullptr;
	CGHNode* m_currSelected = nullptr;
	DirectX::XMFLOAT3 m_rotation = {};
};


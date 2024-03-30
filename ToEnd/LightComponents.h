#pragma once
#include "Component.h"

class COMDirLight : public Component
{
public:
	COMDirLight(CGHNode* node);

	virtual void Release(CGHNode* ndoe) {};
	virtual void Update(CGHNode* node, unsigned int, float delta) override {}
	virtual void RateUpdate(CGHNode* node, unsigned int, float delta) override;
	virtual size_t GetTypeHashCode() override { return s_hashCode; }

private:
	static size_t s_hashCode;
	unsigned int m_lightFlags = 0;
	float m_power = 1.0f;
	float m_nearPlane = 0.0f;
};
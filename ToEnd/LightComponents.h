#pragma once
#include "Component.h"

enum LIGHT_SHADOW_TYPE
{
	LIGHT_SHADOW_NONE,
	LIGHT_SHADOW_HARD,
};

class COMDirLight : public Component
{
public:
	COMDirLight(CGHNode* node);

	virtual void Release(CGHNode* ndoe) {};
	virtual void Update(CGHNode* node, unsigned int, float delta) override;
	virtual void RateUpdate(CGHNode* node, unsigned int, float delta) override {}
	virtual size_t GetTypeHashCode() override { return s_hashCode; }

private:
	static size_t s_hashCode;
	DirectX::XMFLOAT3 m_color;
	LIGHT_SHADOW_TYPE m_shadowType;
	float m_power = 1.0f;
	float m_nearPlane = 0.0f;
};
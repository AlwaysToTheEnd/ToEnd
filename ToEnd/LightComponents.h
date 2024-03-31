#pragma once
#include <basetsd.h>
#include "Component.h"

struct LightData
{
	DirectX::XMFLOAT3 pos;
	float power;
	DirectX::XMFLOAT3 color;
	float pad0;
	DirectX::XMFLOAT3 dir;
	float pad1;
};

class COMDirLight : public Component
{
public:
	COMDirLight(CGHNode* node);

	virtual void Release(CGHNode* ndoe) {};
	virtual void Update(CGHNode* node, unsigned int, float delta) override {}
	virtual void RateUpdate(CGHNode* node, unsigned int, float delta) override;
	virtual size_t GetTypeHashCode() override { return s_hashCode; }
	UINT64 GetLightDataGPU(unsigned int currFrameIndex);

public:
	LightData m_data;

private:
	static size_t s_hashCode;
	unsigned int m_lightIndex = 0;
	unsigned int m_lightFlags = 0;
};
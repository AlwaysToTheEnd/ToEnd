#pragma once
#include <basetsd.h>
#include "Component.h"
#pragma warning(push)
#pragma warning(disable: 26812)

struct LightData
{
	DirectX::XMFLOAT3 pos = {};
	float power = 1.0f;
	DirectX::XMFLOAT3 dir = {};
	float length = 0;
	DirectX::XMFLOAT3 color = { 1.0f, 1.0f, 1.0f };
	float pad1 = 0;
};

class CGHLightComponent : public Component
{
public:
	virtual void Update(CGHNode* node, float delta) = 0;
	enum LIGHT_FLAGS : unsigned int
	{
		LIGHT_FLAG_NONE =0,
		LIGHT_FLAG_SHADOW = 1,
	};

	LightData m_data;

	void SetFlags(LIGHT_FLAGS flags) { m_lightFlags = flags; }
	LIGHT_FLAGS GetFlags() { return static_cast<LIGHT_FLAGS>(m_lightFlags); }

protected:
	unsigned int m_lightFlags = 0;

};

class COMDirLight : public CGHLightComponent
{
public:
	COMDirLight(CGHNode* node);

	virtual void Update(CGHNode* node, float delta) override {}
	virtual void RateUpdate(CGHNode* node, float delta) override;
	virtual void Render(CGHNode* node, unsigned int) override;
	virtual size_t GetTypeHashCode() override { return s_hashCode; }

private:
	virtual void GUIRender_Internal(unsigned int currFrame) override;

private:
	static size_t s_hashCode;
};

class COMPointLight : public CGHLightComponent
{
public:
	COMPointLight(CGHNode* node);

	virtual void Update(CGHNode* node, float delta) override {}
	virtual void RateUpdate(CGHNode* node, float delta) override;
	virtual void Render(CGHNode* node, unsigned int) override;
	virtual size_t GetTypeHashCode() override { return s_hashCode; }

private:
	static size_t s_hashCode;
	
};

#pragma warning(pop)
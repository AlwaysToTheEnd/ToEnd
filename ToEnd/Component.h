#pragma once
#include <string>
#include <vector>
#include <DirectXMath.h>

enum COMPONENTPRIORITY
{
	COMPONENT_TRANSFORM,
	COMPONENT_MATERIAL,
	COMPONENT_SKINNEDMESH,
	COMPONENT_MESH_RENDERER,
	COMPONENT_SKINNEDMESH_RENDERER,
};

class CGHNode;

class Component
{
public:
	virtual void Update(CGHNode* node, float delta) = 0;
	virtual void RateUpdate(CGHNode* node, float delta) = 0;
	virtual size_t GetTypeHashCode() = 0;
	virtual unsigned int GetPriority() = 0;

protected:
};

class COMTransform : public Component
{
public:
	COMTransform();

	virtual void Update(CGHNode* node, float delta) override;
	virtual void RateUpdate(CGHNode* node, float delta) override;
	virtual size_t GetTypeHashCode() override { return s_hashCode; }
	virtual unsigned int GetPriority() override { return COMPONENT_TRANSFORM; }

	void XM_CALLCONV SetPos(DirectX::FXMVECTOR pos);
	void XM_CALLCONV SetScale(DirectX::FXMVECTOR scale);
	void XM_CALLCONV SetRotateQuter(DirectX::FXMVECTOR quterRotate);

private:
	static size_t s_hashCode;
	DirectX::XMFLOAT3 m_pos = {};
	DirectX::XMFLOAT3 m_scale = { 1.0f,1.0f,1.0f };
	DirectX::XMFLOAT4 m_queternion = {};
};

class COMMaterial : public Component
{
public:

private:

};

class COMSkinnedMesh : public Component
{
public:

private:
};
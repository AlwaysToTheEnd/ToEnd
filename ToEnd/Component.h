#pragma once
#include <string>
#include <vector>
#include <DirectXMath.h>
#include "CGHGraphicResource.h"

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
	virtual ~Component() = default;
	virtual void Update(CGHNode* node, unsigned int currFrame, float delta) = 0;
	virtual void RateUpdate(CGHNode* node, unsigned int currFrame, float delta) = 0;
	virtual size_t GetTypeHashCode() = 0;
	virtual unsigned int GetPriority() { return UINT_MAX; }

protected:
};

class COMTransform : public Component
{
public:
	COMTransform();

	virtual void Update(CGHNode* node, unsigned int, float delta) override;
	virtual void RateUpdate(CGHNode* node, unsigned int, float delta) override {}
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

class COMSkinnedMesh : public Component
{
	static const unsigned int MAXNUMBONE = 256;
	static const unsigned int BONEDATASIZE = MAXNUMBONE * 4 * 16;

public:
	COMSkinnedMesh();
	virtual ~COMSkinnedMesh();
	virtual void Update(CGHNode* node, unsigned int currFrame, float delta) override {}
	virtual void RateUpdate(CGHNode* node, unsigned int currFrame, float delta) override;
	virtual size_t GetTypeHashCode() override { return s_hashCode; }
	virtual unsigned int GetPriority() override { return COMPONENT_SKINNEDMESH; }

	void SetMeshData(const CGHMeshDataSet* meshData);
	D3D12_GPU_VIRTUAL_ADDRESS GetBoneData(unsigned int currFrame);

private:
	static size_t s_hashCode;
	const CGHMeshDataSet* m_data;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_boneDatas;
	std::vector<DirectX::XMFLOAT4X4*> m_boneDatasCpu;
};



class COMMaterial : public Component
{
public:

public:
	const CGHMaterial* m_data;

private:

};


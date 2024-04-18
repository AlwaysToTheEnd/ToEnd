#pragma once
#include <string>
#include <vector>
#include "Component.h"
#include "CGHGraphicResource.h"
#include "DX12FontStructs.h"

class COMTransform : public Component
{
public:
	COMTransform(CGHNode* node);

	virtual void Update(CGHNode* node, unsigned int, float delta) override;
	virtual void RateUpdate(CGHNode* node, unsigned int, float delta) override {}
	virtual size_t GetTypeHashCode() override { return s_hashCode; }
	virtual unsigned int GetPriority() override { return COMPONENT_TRANSFORM; }

	void XM_CALLCONV SetPos(DirectX::FXMVECTOR pos);
	void XM_CALLCONV SetScale(DirectX::FXMVECTOR scale);
	void XM_CALLCONV SetRotateQuter(DirectX::FXMVECTOR quterRotate);

	DirectX::FXMVECTOR XM_CALLCONV GetRotateQuter() { return DirectX::XMLoadFloat4(&m_queternion); }

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
	COMSkinnedMesh(CGHNode* node);
	virtual ~COMSkinnedMesh();
	virtual void Release(CGHNode* node) override;
	virtual void Update(CGHNode* node, unsigned int currFrame, float delta) override {}
	virtual void RateUpdate(CGHNode* node, unsigned int currFrame, float delta) override;
	virtual size_t GetTypeHashCode() override { return s_hashCode; }
	virtual unsigned int GetPriority() override { return COMPONENT_SKINNEDMESH; }

	void SetMeshData(const CGHMesh* meshData);
	const CGHMesh* GetMeshData() const { return m_data; }
	D3D12_GPU_VIRTUAL_ADDRESS GetBoneData(unsigned int currFrame);
	void NodeTreeDirty();

private:

private:
	static size_t s_hashCode;
	const CGHMesh* m_data = nullptr;
	bool m_nodeTreeDirty = true;
	std::unordered_map<std::string, const CGHNode*> m_currNodeTree;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_boneDatas;
	std::vector<DirectX::XMFLOAT4X4*> m_boneDatasCpu;
};

class DX12TextureBuffer;

class COMMaterial : public Component
{
public:
	COMMaterial(CGHNode* node);
	virtual ~COMMaterial();
	virtual void Release(CGHNode* node) override;
	virtual void Update(CGHNode* node, unsigned int currFrame, float delta) override {}
	virtual void RateUpdate(CGHNode* node, unsigned int currFrame, float delta) override;
	virtual size_t GetTypeHashCode() override { return s_hashCode; }
	virtual unsigned int GetPriority() override { return COMPONENT_MATERIAL; }

	void SetData(const CGHMaterial* mat);
	void SetTexture(const TextureInfo* textureInfo, unsigned int index);
	const CGHMaterial* GetMaterialData() { return &m_material; }
	UINT64 GetMaterialDataGPU(unsigned int currFrameIndex);
	ID3D12DescriptorHeap* GetTextureHeap() { return m_descHeap.Get(); }

public:
	void* m_shader = nullptr;

private:
	static size_t s_hashCode;
	CGHMaterial m_material;
	unsigned int m_currMaterialIndex = 0;
	DX12TextureBuffer* m_textureBuffer = nullptr;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_descHeap;
};

class CGHRenderer
{
public:
	unsigned int GetRenderID() { return m_renderID + 1; }

protected:
	unsigned int m_renderID = 0;
	unsigned int m_renderFlag = 0;
};

class COMDX12SkinnedMeshRenderer : public Component, public CGHRenderer
{
public:
	COMDX12SkinnedMeshRenderer(CGHNode* node);
	virtual ~COMDX12SkinnedMeshRenderer() = default;

	virtual void Update(CGHNode* node, unsigned int currFrame, float delta) override {}
	virtual void RateUpdate(CGHNode* node, unsigned int currFrame, float delta) override;
	virtual size_t GetTypeHashCode() override { return s_hashCode; }
	virtual unsigned int GetPriority() override { return COMPONENT_SKINNEDMESH_RENDERER; }

private:
	static size_t s_hashCode;
};

class COMUIRenderer : public Component, public CGHRenderer
{
public:
	COMUIRenderer(CGHNode* node);
	virtual ~COMUIRenderer() = default;

	virtual void Update(CGHNode* node, unsigned int currFrame, float delta) override {}
	virtual void RateUpdate(CGHNode* node, unsigned int currFrame, float delta) override;
	virtual size_t GetTypeHashCode() override { return s_hashCode; }
	virtual unsigned int GetPriority() override { return COMPONENT_UI_RENDERER; }

private:
	static size_t s_hashCode;
};

class COMFontRenderer : public Component, public CGHRenderer
{
public:
	COMFontRenderer(CGHNode* node);
	virtual ~COMFontRenderer() = default;

	virtual void Update(CGHNode* node, unsigned int currFrame, float delta) override {}
	virtual void RateUpdate(CGHNode* node, unsigned int currFrame, float delta) override;
	virtual size_t GetTypeHashCode() override { return s_hashCode; }
	virtual unsigned int GetPriority() override { return COMPONENT_CUSTOM; }

	void SetRenderString(const wchar_t* str,
		DirectX::FXMVECTOR color, const DirectX::XMFLOAT3& pos, float scale, float rowPitch);
	void SetPos(const DirectX::XMFLOAT3& pos);
	void SetText(const wchar_t* str);
	void XM_CALLCONV SetColor(DirectX::FXMVECTOR color);
	void SetSize(float size);
	void SetRowPitch(float rowPitch);


private:
	static size_t s_hashCode;
	CGH::DX12RenderString m_renderString;
};

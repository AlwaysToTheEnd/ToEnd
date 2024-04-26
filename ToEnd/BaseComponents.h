#pragma once
#include <string>
#include <vector>
#include <d3d12.h>
#include <windef.h>
#include <DirectXMath.h>
#include <wrl.h>
#include "Component.h"
#include "CGHGraphicResource.h"

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
	void SetPos(const DirectX::XMFLOAT3& pos) { m_pos = pos; }
	void SetScale(const DirectX::XMFLOAT3& scale) { m_scale = scale; }
	void SetRotation(const DirectX::XMFLOAT4& quter) { m_queternion = quter; }

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
	enum RENDER_FLAGS
	{
		RENDER_FLAG_NONE = 0,
		RENDER_FLAG_NON_TARGET_SHADOW = 1,
		RENDER_FLAG_NON_RECEIVE_SHADOW = 2,
		RENDER_FLAG_SHADOW_TWOSIDE = 4,
	};

public:
	CGHRenderer();
	virtual ~CGHRenderer();
	unsigned int GetRenderID() { return m_renderID + 1; }

protected:
	static unsigned int s_currRendererInstancedNum;
	static std::vector<unsigned int> s_renderIDPool;
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

class COMUITransform : public Component
{
public:
	COMUITransform(CGHNode* node) {};

	virtual void Update(CGHNode* node, unsigned int, float delta) override;
	virtual void RateUpdate(CGHNode* node, unsigned int, float delta) override {}
	virtual size_t GetTypeHashCode() override { return s_hashCode; }
	virtual unsigned int GetPriority() override { return COMPONENT_UITRANSFORM; }

	void XM_CALLCONV SetPos(DirectX::FXMVECTOR pos);
	void XM_CALLCONV SetScale(DirectX::FXMVECTOR scale);
	void XM_CALLCONV SetSize(DirectX::FXMVECTOR size);
	void SetPos(const DirectX::XMFLOAT3& pos) { m_pos = pos; }
	void SetScale(const DirectX::XMFLOAT2& scale) { m_scale = scale; }
	void SetSize(const DirectX::XMFLOAT2& size) { m_sizeL = size; }

	DirectX::XMFLOAT2 GetSize() { return m_size; }


private:
	static size_t s_hashCode;
	DirectX::XMFLOAT3 m_pos = {};
	DirectX::XMFLOAT2 m_scale = { 1.0f, 1.0f };
	DirectX::XMFLOAT2 m_sizeL = {};
	DirectX::XMFLOAT2 m_size = {};
};

class COMUIRenderer : public Component, public CGHRenderer
{
public:
	COMUIRenderer(CGHNode* node);
	virtual ~COMUIRenderer() = default;

	virtual void Update(CGHNode* node, unsigned int currFrame, float delta) override {}
	virtual void RateUpdate(CGHNode* node, unsigned int currFrame, float delta) override;
	virtual size_t GetTypeHashCode() override { return s_hashCode; }

	void XM_CALLCONV SetColor(DirectX::FXMVECTOR color) { DirectX::XMStoreFloat4(&m_color, color); m_isTextureBackGound = false; }
	void SetSpriteSubIndex(unsigned int index) { m_spriteSubIndex = index; m_isTextureBackGound = true; }

private:
	static size_t s_hashCode;
	bool m_isTextureBackGound = false;
	DirectX::XMFLOAT4 m_color;
	unsigned int m_spriteSubIndex = 0;
};

class COMFontRenderer : public Component, public CGHRenderer
{
public:
	COMFontRenderer(CGHNode* node);
	virtual ~COMFontRenderer() = default;

	virtual void Update(CGHNode* node, unsigned int currFrame, float delta) override {}
	virtual void RateUpdate(CGHNode* node, unsigned int currFrame, float delta) override;
	virtual size_t GetTypeHashCode() override { return s_hashCode; }

	void SetRenderString(const wchar_t* str, DirectX::FXMVECTOR color, float rowPitch);
	void SetText(const wchar_t* str);
	void XM_CALLCONV SetColor(DirectX::FXMVECTOR color);
	void SetRowPitch(float rowPitch);

private:
	static size_t s_hashCode;
	std::wstring m_str;
	DirectX::XMFLOAT4 m_color = { 0.0f,0.0f, 0.0f, 1.0f };
	float m_rowPitch = 100.0f;
};

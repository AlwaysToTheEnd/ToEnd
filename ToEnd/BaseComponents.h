#pragma once
#include <string>
#include <vector>
#include <d3d12.h>
#include <windef.h>
#include <DirectXMath.h>
#include <wrl.h>
#include "Component.h"
#include "CGHGraphicResource.h"
#include "PipeLineWorkSet.h"

class COMTransform : public Component
{
public:
	COMTransform(CGHNode* node);

	virtual void Update(CGHNode* node, float delta) override;
	virtual void RateUpdate(CGHNode* node, float delta) override {};
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
	virtual void Update(CGHNode* node, float delta) override {}
	virtual void RateUpdate(CGHNode* node, float delta) override;
	virtual void Render(CGHNode* node, unsigned int currFrame) override;
	virtual size_t GetTypeHashCode() override { return s_hashCode; }
	virtual unsigned int GetPriority() override { return COMPONENT_SKINNEDMESH; }

	void SetMeshData(const CGHMesh* meshData);
	const CGHMesh* GetMeshData() const { return m_data; }
	D3D12_GPU_VIRTUAL_ADDRESS GetBoneData(unsigned int currFrame);
	D3D12_GPU_VIRTUAL_ADDRESS GetResultMeshData(MESHDATA_TYPE type);
	ID3D12Resource* GetResultMeshResource() { return m_VNTBResource.Get(); }
	ID3D12DescriptorHeap* GetDescriptorHeap() { return m_srvuavHeap.Get(); }
	
	void NodeTreeDirty();

private:
	static size_t s_hashCode;
	static unsigned int s_srvUavSize;
	const CGHMesh* m_data = nullptr;
	bool m_nodeTreeDirty = true;
	std::unordered_map<std::string, const CGHNode*> m_currNodeTree;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_boneDatas;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_VNTBResource;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_srvuavHeap;
	std::vector<DirectX::XMFLOAT4X4*> m_boneDatasCpu;
};

class DX12TextureBuffer;

class COMMaterial : public Component
{
public:
	COMMaterial(CGHNode* node);
	virtual ~COMMaterial();
	virtual void Release(CGHNode* node) override;
	virtual void Update(CGHNode* node, float delta) override {}
	virtual void RateUpdate(CGHNode* node, float delta) override;
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
	struct MouseAction
	{
		int funcMouseButton = 0;
		int funcMouseState = 0;
		CGHNode* node = nullptr;
		std::function<void(CGHNode*)> func = nullptr;
	};
public:
	enum RENDER_FLAGS
	{
		RENDER_FLAG_NONE = 0,
		RENDER_FLAG_NON_TARGET_SHADOW = 1,
		RENDER_FLAG_NON_RECEIVE_SHADOW = 2,
		RENDER_FLAG_SHADOW_TWOSIDE = 4,
	};

public:
	CGHRenderer(CGHNode* node);
	virtual ~CGHRenderer();
	static void ExcuteMouseAction(unsigned int renderID);
	static void AddGlobalActionCurrFrame(int mousebutton, int mouseState, std::function<void(CGHNode*)> func);

	unsigned int GetRenderID() const { return m_renderID + 1; }
	unsigned int GetRenderFlags() const { return m_renderFlag; }
	void SetParentRender(const CGHRenderer* render); // only for ui, font rendering
	void AddFunc(int mousebutton, int mouseState, CGHNode* node, std::function<void(CGHNode*)> func);

	void RemoveFuncs();

private:
	static int GetMouseTargetState(int button,const void* mouse);

protected:
	static std::unordered_map<unsigned int, std::vector<MouseAction>> s_mouseActions;
	static std::vector<CGHRenderer::MouseAction> s_globalActions;
	static std::vector<CGHNode*> s_hasNode;
	static std::vector<unsigned int> s_renderIDPool;
	static unsigned int s_currRendererInstancedNum;
	unsigned int m_parentRenderID = 0;
	unsigned int m_renderID = 0;
	unsigned int m_renderFlag = 0;
};

class COMDX12SkinnedMeshRenderer : public Component, public CGHRenderer
{
public:
	COMDX12SkinnedMeshRenderer(CGHNode* node);
	virtual ~COMDX12SkinnedMeshRenderer() = default;

	virtual void Update(CGHNode* node,float delta) override {}
	virtual void RateUpdate(CGHNode* node, float delta) override {};
	virtual void Render(CGHNode* node, unsigned int) override;
	virtual size_t GetTypeHashCode() override { return s_hashCode; }
	virtual unsigned int GetPriority() override { return COMPONENT_SKINNEDMESH_RENDERER; }

	void SetPSOW(const char* name);
	void SetGroupRender(bool value) { m_isGroupRenderTarget = value; }

private:
	static const PipeLineWorkSet* s_skinnedMeshBoneUpdateCompute;
	const PipeLineWorkSet* m_currPsow  = nullptr;
	static size_t s_hashCode;
	bool m_isGroupRenderTarget = false;
};

class COMUITransform : public Component
{
public:
	COMUITransform(CGHNode* node) {};

	virtual void Update(CGHNode* node, float delta) override;
	virtual void RateUpdate(CGHNode* node, float delta) override {}
	virtual size_t GetTypeHashCode() override { return s_hashCode; }
	virtual unsigned int GetPriority() override { return COMPONENT_UITRANSFORM; }

	void XM_CALLCONV SetPos(DirectX::FXMVECTOR pos);
	void XM_CALLCONV SetSize(DirectX::FXMVECTOR size);
	void SetPos(const DirectX::XMFLOAT3& pos) { m_pos = pos; }
	void SetSize(const DirectX::XMFLOAT2& size) { m_size = size; }

	DirectX::XMFLOAT2 GetSize() { return m_size; }

private:
	static size_t s_hashCode;
	DirectX::XMFLOAT3 m_pos = {};
	DirectX::XMFLOAT2 m_size = {};
};

class COMUIRenderer : public Component, public CGHRenderer
{
public:
	COMUIRenderer(CGHNode* node);
	virtual ~COMUIRenderer() = default;

	virtual void Update(CGHNode* node, float delta) override {}
	virtual void RateUpdate(CGHNode* node, float delta) override {}
	virtual void Render(CGHNode* node, unsigned int) override;
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

	virtual void Update(CGHNode* node, float delta) override {}
	virtual void RateUpdate(CGHNode* node, float delta) override;
	virtual void Render(CGHNode* node, unsigned int) override;
	virtual size_t GetTypeHashCode() override { return s_hashCode; }

	void SetRenderString(const wchar_t* str, DirectX::FXMVECTOR color, float rowPitch);
	void SetText(const wchar_t* str);
	void SetFontSize(float size) { m_fontSize = size; }
	void XM_CALLCONV SetColor(DirectX::FXMVECTOR color);
	void SetRowPitch(float rowPitch);

private:
	static size_t s_hashCode;
	std::wstring m_str;
	DirectX::XMFLOAT4 m_color = { 0.0f,0.0f, 0.0f, 1.0f };
	float m_fontSize = 10.0f;
	float m_rowPitch = 100.0f;
};

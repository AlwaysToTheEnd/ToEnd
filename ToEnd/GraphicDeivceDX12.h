#pragma once

#include <memory>
#include <vector>
#include <unordered_map>
#include <string>
#include <wrl.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <DirectXMath.h>
#include <functional>
#include <algorithm>
#include "DX12UploadBuffer.h"
#include "CGHBaseClass.h"
#include "DX12FontStructs.h"

#include "../Common/Source/CGHUtil.h"

using Microsoft::WRL::ComPtr;
class DX12SwapChain;
class Camera;
class DX12SMAA;

struct DX12PassConstants
{
	DirectX::XMFLOAT4X4		view = CGH::IdentityMatrix;
	DirectX::XMFLOAT4X4		invView = CGH::IdentityMatrix;
	DirectX::XMFLOAT4X4		proj = CGH::IdentityMatrix;
	DirectX::XMFLOAT4X4		invProj = CGH::IdentityMatrix;
	DirectX::XMFLOAT4X4		viewProj = CGH::IdentityMatrix;
	DirectX::XMFLOAT4X4		invViewProj = CGH::IdentityMatrix;
	DirectX::XMFLOAT4X4		rightViewProj = CGH::IdentityMatrix;
	DirectX::XMFLOAT4X4		orthoMatrix = CGH::IdentityMatrix;
	unsigned int			renderTargetSizeX = 0;
	unsigned int			renderTargetSizeY = 0;
	DirectX::XMFLOAT2		invRenderTargetSize = { 0.0f, 0.0f };
	DirectX::XMFLOAT4		ambientLight = { 0.0f, 0.0f, 0.0f, 1.0f };
	DirectX::XMFLOAT3		eyePosW = { 0.0f, 0.0f, 0.0f };
	unsigned int			samplerIndex = 0;
	DirectX::XMFLOAT2		mousePos = {};
	DirectX::XMFLOAT2		pad = {};
};

class GraphicDeviceDX12
{
	struct DX12RenderQueue
	{
		std::vector<std::pair<CGHNode*, unsigned int>> queue;
	};

	struct PipeLineWorkSet
	{
		ID3D12PipelineState* pso = nullptr;
		ID3D12RootSignature* rootSig = nullptr;
		DX12RenderQueue* renderQueue = nullptr;
		std::function<void(ID3D12GraphicsCommandList* cmd)> baseGraphicCmdFunc;
		std::function<void(ID3D12GraphicsCommandList* cmd, CGHNode* node, unsigned int renderFlag)> nodeGraphicCmdFunc;
	};

	struct ShadowMap
	{
		ComPtr<ID3D12Resource> resource;
		ComPtr<ID3D12DescriptorHeap> texDSVHeap;
		DirectX::XMFLOAT4X4 lightViewProj;
		unsigned int srvHeapIndex = 0;
	};

	struct DX12DirLightData
	{
		DirectX::XMFLOAT3 dir = { 0.0f, 0.0f, -1.0f };
		float power = 1.0f;
		DirectX::XMFLOAT3 color = {};
		int shadowMapIndex = -1;
		DirectX::XMFLOAT4X4 shadowMapMat = CGH::IdentityMatrix;
	};

	struct UIInfo
	{
		static const unsigned int maxNumUI = 4096;

		unsigned int uiGraphicType = 0;
		DirectX::XMFLOAT3 pos;
		DirectX::XMFLOAT4 color;
		DirectX::XMFLOAT2 uvLT = { 0.0f, 0.0f };
		DirectX::XMFLOAT2 uvRB = { 1.0f, 1.0f };
		DirectX::XMFLOAT2 size;
		unsigned int renderID = 0;
		float pad0;
	};

	enum DEFERRED_TEXTURE
	{
		DEFERRED_TEXTURE_DIFFUSE,
		DEFERRED_TEXTURE_NORMAL,
		DEFERRED_TEXTURE_MRA,
		DEFERRED_TEXTURE_EMISSIONCOLOR,
		DEFERRED_TEXTURE_RENDERID,
		DEFERRED_TEXTURE_NUM,
	};

	DXGI_FORMAT m_deferredFormat[DEFERRED_TEXTURE_NUM] = {
		DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_R11G11B10_FLOAT,
		DXGI_FORMAT_R11G11B10_FLOAT,DXGI_FORMAT_R11G11B10_FLOAT,
		DXGI_FORMAT_R16_UINT };

public:
	static GraphicDeviceDX12* GetGraphic();
	static void CreateDeivce(HWND hWnd, int windowWidth, int windowHeight);
	static void DeleteDeivce();
	static ID3D12Device* GetDevice();

	ID3D12CommandQueue* GetCommandQueue() { return m_commandQueue.Get(); }
	unsigned int GetNumFrameResource() { return m_numFrameResource; }
	unsigned int GetCurrFrameIndex() { return m_currFrame; }
	UINT16 GetCurrMouseTargetRenderID() { return m_currMouseTargetRenderID; }

	D3D12_CPU_DESCRIPTOR_HANDLE GetCurrPresentRTV();
	D3D12_CPU_DESCRIPTOR_HANDLE GetPresentDSV();
	D3D12_GPU_VIRTUAL_ADDRESS GetCurrMainPassCBV();
	const D3D12_RECT& GetBaseScissorRect() { return m_scissorRect; }
	const D3D12_VIEWPORT& GetBaseViewport() { return m_screenViewport; }

	GraphicDeviceDX12(const GraphicDeviceDX12& rhs) = delete;
	GraphicDeviceDX12& operator=(const GraphicDeviceDX12& rhs) = delete;

	void Update(float delta, const Camera* camera);

	void RenderMesh(CGHNode* node, unsigned int renderFlag);
	void RenderSkinnedMesh(CGHNode* node, unsigned int renderFlag);
	void RenderLight(CGHNode* node, unsigned int lightFlags, size_t lightType);
	void RenderUI(const DirectX::XMFLOAT3& pos, const DirectX::XMFLOAT2 size, const DirectX::XMFLOAT4 color, unsigned int renderID);
	void RenderUI(const DirectX::XMFLOAT3& pos, const DirectX::XMFLOAT2 size, unsigned int spriteTextureSubIndex, float alpha, unsigned int renderID);
	void RenderString(const wchar_t* str, const DirectX::XMFLOAT4& color, const DirectX::XMFLOAT3& pos, float size, float rowPitch);
	void RenderBegin();
	void RenderEnd();

	void OnResize(int windowWidth, int windowHeight);
	void LoadMeshDataFile(const char* filePath, bool triangleCw, std::vector<CGHMesh>* outMeshSet,
		std::vector<CGHMaterial>* outMaterials = nullptr, std::vector<CGHNode>* outNode = nullptr);

private:
	GraphicDeviceDX12();
	~GraphicDeviceDX12();
	void BaseRender();
	void CreateRenderResources();
	void ReservedUIInfoSort();

	void Init(HWND hWnd, int windowWidth, int windowHeight);
	void FlushCommandQueue();

	void BuildPso();
	void CreateDeferredTextures(int windowWidth, int windowHeight);

	ID3D12CommandAllocator* GetCurrRenderBeginCommandAllocator();
	ID3D12CommandAllocator* GetCurrRenderEndCommandAllocator();

private:
	enum PSOWORKSETLIST
	{
		PSOW_DEFERRED_SKINNEDMESH = 0,
		PSOW_SHADOWMAP_WRITE,
		PSOW_DEFERRED_LIGHT_DIR,
		PSOW_SMAA_EDGE_RENDER,
		PSOW_SMAA_BLEND_RENDER,
		PSOW_SMAA_NEIBLEND_RENDER,
		PSOW_UI_RENDER,
		PSOW_FONT_RENDER,
		PSOW_UI_RENDERID_RENDER,
		PSOW_TEX_DEBUG,
		PSOW_NUM
	};

	enum RENDERQUEUE
	{
		RENDERQUEUE_MESH = 0,
		RENDERQUEUE_SKINNEDMESH,
		RENDERQUEUE_LIGHT_DIR,
		RENDERQUEUE_LIGHT_POINT,
		RENDERQUEUE_NUM
	};

	void BuildDeferredSkinnedMeshPipeLineWorkSet();
	void BuildShadowMapWritePipeLineWorkSet();
	void BuildDeferredLightDirPipeLineWorkSet();
	void BuildSMAARenderPipeLineWorkSet();
	void BuildFontRenderPipeLineWorkSet();
	void BuildUIRenderPipeLineWorkSet();
	void BuildUIRenderIDRenderPipeLineWorkSet();
	void BuildTextureDataDebugPipeLineWorkSet();

private:
	static GraphicDeviceDX12* s_Graphic;

	D3D12_VIEWPORT					m_screenViewport = {};
	D3D12_RECT						m_scissorRect = {};
	D3D_DRIVER_TYPE					m_d3dDriverType = D3D_DRIVER_TYPE::D3D_DRIVER_TYPE_UNKNOWN;

	ComPtr<ID3D12Device>			m_d3dDevice;

	DXGI_FORMAT						m_backBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
	unsigned int					m_rtvSize = 0;
	unsigned int					m_srvcbvuavSize = 0;
	unsigned int					m_dsvSize = 0;

	const unsigned int				m_numFrameResource = CGH::NumFrameresource;
	unsigned int					m_currFrame = 0;
	unsigned long long				m_currentFence = 0;
	ComPtr<ID3D12Fence1>			m_fence;
	std::vector<unsigned long long>	m_fenceCounts;

	DirectX::XMFLOAT4X4				m_projMat = CGH::IdentityMatrix;
	DirectX::XMFLOAT4X4				m_cameraViewProjMat = CGH::IdentityMatrix;
	DirectX::XMFLOAT3				m_rayOrigin = { 0,0,0 };
	DirectX::XMFLOAT3				m_ray = { 0,0,0 };
	DirectX::XMFLOAT3				m_cameraTargetPos = { 0,0,0 };
	float							m_cameraDistance = 0;

	DX12SwapChain*					m_swapChain = nullptr;

	PipeLineWorkSet					m_PSOWorkSets[PSOW_NUM];
	DX12RenderQueue					m_renderQueues[RENDERQUEUE_NUM];

	ComPtr<ID3D12Resource>			m_deferredResources[DEFERRED_TEXTURE_NUM] = {};
	ComPtr<ID3D12DescriptorHeap>	m_deferredRTVHeap;

	UINT16							m_currMouseTargetRenderID = 0;
	ComPtr<ID3D12Resource>			m_renderIDatMouseRead;

	std::vector<UIInfo>				m_reservedUIInfos;
	std::vector<UIInfo*>			m_uiInfoMapped;
	ComPtr<ID3D12Resource>			m_uiInfoDatas;
	ComPtr<ID3D12Resource>			m_uiSpriteTexture;
	ComPtr<ID3D12DescriptorHeap>	m_uiSpriteSRVHeap;

	const unsigned int						m_maxNumChar = 4096;
	unsigned int							m_numRenderChar = 0;
	std::vector<CGH::CharInfo*>				m_charInfoMapped;
	Microsoft::WRL::ComPtr<ID3D12Resource>	m_charInfos;

	std::unordered_map<void*, ComPtr<ID3D12Resource>>	m_resultVertexPosBuffers;
	ComPtr<ID3D12Resource>								m_resultVertexPosUABuffer;

	const unsigned int										m_numMaxShadowMap = 8;
	const unsigned int										m_numMaxDirLight = 32;
	unsigned char											m_currNumShadowMap = 0;
	unsigned int											m_numDirLight = 0;
	std::unique_ptr<DX12UploadBuffer<DX12DirLightData>>		m_dirLightDatas;
	std::unique_ptr<DX12UploadBuffer<DirectX::XMMATRIX>>	m_shadowPassCB;
	std::unordered_map<void*, ShadowMap>					m_dirLightShadowMaps;
	ComPtr<ID3D12DescriptorHeap>							m_SRVHeap;

	DX12SMAA*												m_smaa = nullptr;
	float													m_testThreshold = 0.05f;

	ComPtr<ID3D12GraphicsCommandList>									m_cmdList;
	ComPtr<ID3D12GraphicsCommandList>									m_dataLoaderCmdList;
	std::vector<ComPtr<ID3D12CommandAllocator>>							m_cmdListAllocs;
	std::vector<std::unique_ptr<DX12UploadBuffer<DX12PassConstants>>>	m_passCBs;
	ComPtr<ID3D12CommandQueue>											m_commandQueue;
};
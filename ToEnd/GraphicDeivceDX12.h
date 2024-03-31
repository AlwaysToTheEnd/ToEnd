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
#include "DX12UploadBuffer.h"
#include "CGHGraphicResource.h"
#include "CGHBaseClass.h"

#include "../Common/Source/CGHUtil.h"

using Microsoft::WRL::ComPtr;
class DX12SwapChain;
class Camera;

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
		std::vector<std::pair<CGHNode*,unsigned int>> queue;
	};

	struct PipeLineWorkSet
	{
		ID3D12PipelineState* pso = nullptr;
		ID3D12RootSignature* rootSig = nullptr;
		std::function<void(ID3D12GraphicsCommandList* cmd)> baseGraphicCmdFunc;
		std::function<void(ID3D12GraphicsCommandList* cmd, CGHNode* node, unsigned int renderFlag)> nodeGraphicCmdFunc;
	};

	enum PIPELINEWORKLIST
	{
		PIPELINE_MESH,
		PIPELINE_SKINNEDMESH,
		PIPELINE_SHADOW,
		PIPELINE_LIGHT,
		PIPELINE_WORK_NUM
	};

	enum PIPELINELIGHTLIST
	{
		PIPELINELIGHT_DIR,
		PIPELINELIGHT_POINT,
		PIPELINELIGHT_SPOT,
		PIPELINELIGHT_NUM
	};

	enum DEFERRED_TEXTURE
	{
		DEFERRED_TEXTURE_DIFFUSE,
		DEFERRED_TEXTURE_NORMAL,
		DEFERRED_TEXTURE_MRE,
		DEFERRED_TEXTURE_AFA,
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

	D3D12_CPU_DESCRIPTOR_HANDLE GetCurrPresentRTV();
	D3D12_CPU_DESCRIPTOR_HANDLE GetPresentDSV();
	D3D12_GPU_VIRTUAL_ADDRESS GetCurrMainPassCBV();
	const D3D12_RECT& GetBaseScissorRect() { return m_scissorRect; }
	const D3D12_VIEWPORT& GetBaseViewport() { return m_screenViewport; }

	GraphicDeviceDX12(const GraphicDeviceDX12& rhs) = delete;
	GraphicDeviceDX12& operator=(const GraphicDeviceDX12& rhs) = delete;

	void Update(float delta, const Camera* camera);

	void RenderBegin();
	void RenderMesh(CGHNode* node, unsigned int renderFlag);
	void RenderSkinnedMesh(CGHNode* node, unsigned int renderFlag);
	void RenderLight(CGHNode* node, unsigned int lightFlags, size_t lightType);
	void RenderUI(CGHNode* node, unsigned int renderFlag);
	void RenderEnd();

	void OnResize(int windowWidth, int windowHeight);
	void LoadMeshDataFile(const char* filePath, std::vector<CGHMesh>* outMeshSet, 
		std::vector<CGHMaterial>* outMaterials = nullptr, std::vector<CGHNode>* outNode = nullptr);

private:
	GraphicDeviceDX12() = default;
	void BaseRender();
	void LightRender();

	void Init(HWND hWnd, int windowWidth, int windowHeight);
	void FlushCommandQueue();

	void BuildPso();
	void CreateDeferredTextures(int windowWidth, int windowHeight);

	ID3D12CommandAllocator*		GetCurrRenderBeginCommandAllocator();
	ID3D12CommandAllocator*		GetCurrRenderEndCommandAllocator();

private:
	static GraphicDeviceDX12*		s_Graphic;

	D3D12_VIEWPORT					m_screenViewport = {};
	D3D12_RECT						m_scissorRect = {};
	D3D_DRIVER_TYPE					m_d3dDriverType = D3D_DRIVER_TYPE::D3D_DRIVER_TYPE_UNKNOWN;

	ComPtr<ID3D12Device>			m_d3dDevice;

	const unsigned int				m_numFrameResource = CGH::NumFrameresource;
	unsigned int					m_currFrame = 0;
	unsigned long long				m_currentFence = 0;
	ComPtr<ID3D12Fence1>			m_fence;
	std::vector<unsigned long long>	m_fenceCounts;

	DirectX::XMFLOAT4X4				m_projMat = CGH::IdentityMatrix;
	DirectX::XMFLOAT3				m_rayOrigin = { 0,0,0 };
	DirectX::XMFLOAT3				m_ray = { 0,0,0 };

	DXGI_FORMAT						m_backBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
	DX12SwapChain*					m_swapChain = nullptr;

	ID3D12PipelineState*			m_currPipeline = nullptr;
	DX12RenderQueue					m_meshRenderQueue;
	DX12RenderQueue					m_skinnedMeshRenderQueue;
	DX12RenderQueue					m_uiRenderQueue;
	DX12RenderQueue					m_dirLightRenderQueue;

	unsigned int					m_rtvSize = 0;
 	ComPtr<ID3D12Resource>			m_deferredResources[DEFERRED_TEXTURE_NUM] = {};
	ComPtr<ID3D12DescriptorHeap>	m_deferredRTVHeap;
	ComPtr<ID3D12DescriptorHeap>	m_deferredBaseSRVHeap;
	ComPtr<ID3D12Resource>			m_lightDummyVertexBuffer;

	ComPtr<ID3D12GraphicsCommandList>									m_cmdList;
	ComPtr<ID3D12GraphicsCommandList>									m_dataLoaderCmdList;
	std::vector<ComPtr<ID3D12CommandAllocator>>							m_cmdListAllocs;
	std::vector<std::unique_ptr<DX12UploadBuffer<DX12PassConstants>>>	m_passCBs;
	std::vector<PipeLineWorkSet>										m_PSOs;
	std::vector<PipeLineWorkSet>										m_lightPSOs;
	ComPtr<ID3D12CommandQueue>											m_commandQueue;
};
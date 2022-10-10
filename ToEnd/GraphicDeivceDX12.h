#pragma once

#include <memory>
#include <vector>
#include <unordered_map>
#include <string>
#include <wrl.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <DirectXMath.h>
#include "DX12UploadBuffer.h"
#include "../Common/Source/CGHUtil.h"

using Microsoft::WRL::ComPtr;
class DX12SwapChain;
class Camera;

struct DX12GraphicResource
{
	Microsoft::WRL::ComPtr<ID3D12Resource>  resource;
	D3D12_CPU_DESCRIPTOR_HANDLE				cpuHandle = {};
};

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
	DirectX::XMFLOAT2		mousePos;
	DirectX::XMFLOAT2		pad;
};


class GraphicDeviceDX12
{
public:
	static GraphicDeviceDX12* GetGraphic();
	static ID3D12Device* GetDevice();
	static void CreateDeivce(HWND hWnd, int windowWidth, int windowHeight);
	static void DeleteDeivce();

	GraphicDeviceDX12(const GraphicDeviceDX12& rhs) = delete;
	GraphicDeviceDX12& operator=(const GraphicDeviceDX12& rhs) = delete;

	void Update(float delta, const Camera* camera);

	void RenderBegin();
	void LightRenderBegin();
	void RenderEnd();

	void OnResize(int windowWidth, int windowHeight);

private:
	GraphicDeviceDX12() = default;

	void Init(HWND hWnd, int windowWidth, int windowHeight);
	void FlushCommandQueue();

	ID3D12GraphicsCommandList*	GetCurrCommandList();
	ID3D12CommandAllocator*		GetCurrCommandAllocator();

private:
	static GraphicDeviceDX12*	s_Graphic;

	D3D12_VIEWPORT				m_screenViewport;
	D3D12_RECT					m_scissorRect;
	D3D_DRIVER_TYPE				m_d3dDriverType = D3D_DRIVER_TYPE::D3D_DRIVER_TYPE_UNKNOWN;

	ComPtr<ID3D12Device>		m_d3dDevice;

	const unsigned int				m_numFrameResource = 3;
	unsigned int					m_currFrame = 0;
	unsigned long long				m_currentFence = 0;
	ComPtr<ID3D12Fence>				m_fence;
	std::vector<unsigned long long>	m_fenceCounts;

	DirectX::XMFLOAT4X4				m_projMat;
	DirectX::XMFLOAT3				m_rayOrigin;
	DirectX::XMFLOAT3				m_ray;

	DXGI_FORMAT						m_backBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
	DX12SwapChain*					m_swapChain = nullptr;

	std::vector<ComPtr<ID3D12GraphicsCommandList>>						m_cmdLists;
	std::vector<ComPtr<ID3D12CommandAllocator>>							m_cmdListAllocs;
	std::vector<std::unique_ptr<DX12UploadBuffer<DX12PassConstants>>>	m_passCBs;
	std::unordered_map<std::string,
		Microsoft::WRL::ComPtr<ID3D12Resource>>							m_dx12resources;

	ComPtr<ID3D12CommandQueue>											m_commandQueue;
};
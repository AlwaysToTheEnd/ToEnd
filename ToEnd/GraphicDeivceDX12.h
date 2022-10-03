#pragma once

#include <memory>
#include <vector>
#include <wrl.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <DirectXMath.h>

using Microsoft::WRL::ComPtr;

class GraphicDeviceDX12
{
public:
	static GraphicDeviceDX12* GetGraphic();
	static ID3D12Device* GetDevice();
	static void CreateDeivce(HWND hWnd, int windowWidth, int windowHeight);
	static void DeleteDeivce();
	void OnResize(int windowWidth, int windowHeight);

	void RenderBegin();
	void RenderEnd();

private:
	GraphicDeviceDX12() = default;
	GraphicDeviceDX12(const GraphicDeviceDX12 &rhs) = delete;
	GraphicDeviceDX12& operator=(const GraphicDeviceDX12& rhs) = delete;

	void Init(HWND hWnd, int windowWidth, int windowHeight);
	void FlushCommandQueue();

private:
	static GraphicDeviceDX12* s_Graphic;

	D3D12_VIEWPORT			m_screenViewport;
	D3D12_RECT				m_scissorRect;
	D3D_DRIVER_TYPE			m_d3dDriverType = D3D_DRIVER_TYPE::D3D_DRIVER_TYPE_UNKNOWN;

	ComPtr<ID3D12Device>	m_d3dDevice;

	ComPtr<ID3D12Fence>		m_fence;
	unsigned long long		m_currentFence = 0;

	const unsigned int		m_numFrameResource = 3;
	unsigned int			m_currFrame = 0;

	ComPtr<ID3D12CommandQueue>			m_commandQueue;
	ComPtr<ID3D12CommandAllocator>		m_directCmdListAlloc;
	ComPtr<ID3D12GraphicsCommandList>	m_commandList;

	std::vector<ComPtr<ID3D12CommandAllocator>>				m_cmdListAllocs;
	std::vector<ComPtr<ID3D12CommandList>>					m_cmdLists;

	int														m_currBackBufferIndex = 0;
	const int												m_numBackBuffer = 2;
	DXGI_FORMAT												m_backBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
	Microsoft::WRL::ComPtr<IDXGIFactory4>					m_dxgiFactory;
	Microsoft::WRL::ComPtr<IDXGISwapChain>					m_swapChain;
	std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>>		m_backBufferResource;
	Microsoft::WRL::ComPtr<ID3D12Resource>					m_depthStencil;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>			m_backBufferRtvHeap;
};
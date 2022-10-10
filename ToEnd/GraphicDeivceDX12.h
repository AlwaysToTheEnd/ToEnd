#pragma once

#include <memory>
#include <vector>
#include <unordered_map>
#include <string>
#include <wrl.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <DirectXMath.h>

using Microsoft::WRL::ComPtr;
class DX12SwapChain;

struct DX12GraphicResource
{
	Microsoft::WRL::ComPtr<ID3D12Resource>  resource;
	D3D12_CPU_DESCRIPTOR_HANDLE				cpuHandle = {};
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

	void OnResize(int windowWidth, int windowHeight);
	void RenderBegin();
	void LightRenderBegin();
	void RenderEnd();

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

	DXGI_FORMAT						m_backBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
	DX12SwapChain*					m_swapChain = nullptr;

	std::vector<ComPtr<ID3D12GraphicsCommandList>>	m_cmdLists;
	std::vector<ComPtr<ID3D12CommandAllocator>>		m_cmdListAllocs;
	ComPtr<ID3D12CommandQueue>						m_commandQueue;
	std::unordered_map<std::string, 
		Microsoft::WRL::ComPtr<ID3D12Resource>>		m_dx12resources;
};
#pragma once
#include <d3d12.h>
#include <dxgi1_4.h>
#include <wrl.h>
#include <vector>
#include <DirectXColors.h>

#pragma comment(lib, "dxgi.lib")

class DX12SwapChain
{
public:
	DX12SwapChain();
	virtual ~DX12SwapChain();

	void CreateDXGIFactory(ID3D12Device** device);
	void CreateSwapChain(HWND handle, ID3D12CommandQueue* queue, DXGI_FORMAT renderTarget,
						 unsigned int x, unsigned int y, unsigned int numSwapChain);
	
	void ReSize(ID3D12GraphicsCommandList* cmd, unsigned int x, unsigned int y);

	void RenderBegin(ID3D12GraphicsCommandList* cmd, const float clearColor[4]);
	void RenderEnd(ID3D12GraphicsCommandList* cmd);
	void Present();
	void ClearDS(ID3D12GraphicsCommandList* cmd);

	D3D12_CPU_DESCRIPTOR_HANDLE CurrRTV() const;
	D3D12_CPU_DESCRIPTOR_HANDLE GetDSV() const;
	ID3D12Resource* GetDSResource() const;

private:
	void CreateResources(unsigned int x, unsigned int y);
	void CreateResourceViews();

private:
	ID3D12Device*											m_device;
	DXGI_FORMAT												m_presentBufferFormat;
	unsigned int											m_numSwapBuffer;
	unsigned int											m_currBackBufferIndex;
	unsigned int											m_rtvDescriptorSize;
	unsigned int											m_dsvDescriptorSize;
	unsigned int											m_clientWidth;
	unsigned int											m_clientHeight;

	Microsoft::WRL::ComPtr<IDXGIFactory4>					m_dxgiFactory;
	Microsoft::WRL::ComPtr<IDXGISwapChain>					m_swapChain;

	std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>>		m_renderTargets;
	Microsoft::WRL::ComPtr<ID3D12Resource>					m_depthStencil;

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>			m_rtvHeap;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>			m_dsvHeap;
	float													m_zero[4] = {0,0,0,1.0f};
	float													m_minorIntiger[4] = {-1.0f, -1.0f, -1.0f, -1.0f };
};
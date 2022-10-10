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
	enum BUFFER_RESURECE_TYPE
	{
		GBUFFER_RESOURCE_COLORS,
		GBUFFER_RESOURCE_NORMAL,
		GBUFFER_RESOURCE_SPECPOWER,
		GBUFFER_RESOURCE_OBJECTID,
		GBUFFER_RESOURCE_DS,
		GBUFFER_RESOURCE_PRESENT,
		GBUFFER_RESOURCE_COUNT
	};

public:
	DX12SwapChain();
	virtual ~DX12SwapChain();

	void CreateDXGIFactory(ID3D12Device** device);
	void CreateSwapChain(HWND handle, ID3D12CommandQueue* queue, DXGI_FORMAT renderTarget,
						 unsigned int x, unsigned int y, unsigned int numSwapChain);
	
	void ReSize(ID3D12GraphicsCommandList* cmd, unsigned int x, unsigned int y);
	void GbufferSetting(ID3D12GraphicsCommandList* cmd);
	void PresentRenderTargetSetting(ID3D12GraphicsCommandList* cmd, const float clearColor[4]);
	void RenderEnd(ID3D12GraphicsCommandList* cmd);
	void Present();

	int  GetPixelFuncMapData(UINT x, UINT y);

	ID3D12DescriptorHeap*		GetSrvHeap() { return m_SRVHeap.Get(); }
	D3D12_CPU_DESCRIPTOR_HANDLE CurrRTV(BUFFER_RESURECE_TYPE type) const;
	D3D12_CPU_DESCRIPTOR_HANDLE GetDSV() const;
	D3D12_GPU_DESCRIPTOR_HANDLE GetSRVsGPU() const;

private:
	void CreateResources(unsigned int x, unsigned int y);
	void CreateResourceViews();

private:
	ID3D12Device*											m_Device;
	DXGI_FORMAT												m_NormalBufferFormat;
	DXGI_FORMAT												m_SpecPowBufferFormat;
	DXGI_FORMAT												m_ObjectIDFormat;
	DXGI_FORMAT												m_ColorBufferFormat;
	DXGI_FORMAT												m_PresentBufferFormat;
	unsigned int											m_NumSwapBuffer;
	unsigned int											m_CurrBackBufferIndex;
	unsigned int											m_RTVDescriptorSize;
	unsigned int											m_DSVDescriptorSize;
	unsigned int											m_SRVDescriptorSize;
	unsigned int											m_ClientWidth;
	unsigned int											m_ClientHeight;

	Microsoft::WRL::ComPtr<IDXGIFactory4>					m_DxgiFactory;
	Microsoft::WRL::ComPtr<IDXGISwapChain>					m_SwapChain;

	std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>>		m_Resources;
	Microsoft::WRL::ComPtr<ID3D12Resource>					m_UIDepthStencil;
	Microsoft::WRL::ComPtr<ID3D12Resource>					m_PixelFuncReadBack;
	UINT64													m_PixelFuncReadBackRowPitch;

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>			m_RTVHeap;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>			m_DSVHeap;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>			m_SRVHeap;
	float													m_Zero[4] = {0,0,0,1.0f};
	float													m_MinorIntiger[4] = {-1.0f, -1.0f, -1.0f, -1.0f };
};
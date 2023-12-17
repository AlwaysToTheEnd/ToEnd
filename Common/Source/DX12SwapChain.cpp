#include "d3dx12.h"
#include "DX12SwapChain.h"
#include "CGHUtil.h"
#include "DxException.h"

using Microsoft::WRL::ComPtr;

DX12SwapChain::DX12SwapChain()
	: m_device(nullptr)
	, m_presentBufferFormat(DXGI_FORMAT_UNKNOWN)
	, m_numSwapBuffer(2)
	, m_currBackBufferIndex(0)
	, m_rtvDescriptorSize(0)
	, m_dsvDescriptorSize(0)
	, m_clientWidth(0)
	, m_clientHeight(0)
{
}

DX12SwapChain::~DX12SwapChain()
{


}

void DX12SwapChain::CreateDXGIFactory(ID3D12Device** device)
{
	ThrowIfFailed(CreateDXGIFactory1(IID_PPV_ARGS(m_dxgiFactory.GetAddressOf())));

	if (*device == nullptr)
	{
		ComPtr<IDXGIAdapter> pWarpAdapter;
		ThrowIfFailed(m_dxgiFactory->EnumWarpAdapter(IID_PPV_ARGS(pWarpAdapter.GetAddressOf())));

		ThrowIfFailed(D3D12CreateDevice(pWarpAdapter.Get(), D3D_FEATURE_LEVEL_12_1, IID_PPV_ARGS(device)));
	}

	m_device = *device;

	m_rtvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	m_dsvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
}

void DX12SwapChain::CreateSwapChain(HWND handle, ID3D12CommandQueue* queue, DXGI_FORMAT renderTarget,
	unsigned int x, unsigned int y, unsigned int numSwapBuffer)
{
	m_numSwapBuffer = numSwapBuffer;
	m_presentBufferFormat = renderTarget;

	DXGI_SWAP_CHAIN_DESC sd;
	sd.BufferDesc.Width = x;
	sd.BufferDesc.Height = y;
	sd.BufferDesc.RefreshRate.Numerator = 60;
	sd.BufferDesc.RefreshRate.Denominator = 1;
	sd.BufferDesc.Format = m_presentBufferFormat;
	sd.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UPPER_FIELD_FIRST;
	sd.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
	sd.SampleDesc.Count = 1;
	sd.SampleDesc.Quality = 0;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.BufferCount = numSwapBuffer;
	sd.OutputWindow = handle;
	sd.Windowed = true;
	sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

	ThrowIfFailed(m_dxgiFactory->CreateSwapChain(queue, &sd, m_swapChain.GetAddressOf()));

	m_renderTargets.resize(static_cast<size_t>(m_numSwapBuffer));

	D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc;
	rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	rtvHeapDesc.NodeMask = 0;
	rtvHeapDesc.NumDescriptors = CGH::SizeTTransUINT(m_renderTargets.size());
	rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;

	D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc;
	dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	dsvHeapDesc.NodeMask = 0;
	dsvHeapDesc.NumDescriptors = 1;
	dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;

	ThrowIfFailed(m_device->CreateDescriptorHeap(&rtvHeapDesc,
		IID_PPV_ARGS(m_rtvHeap.GetAddressOf())));
	ThrowIfFailed(m_device->CreateDescriptorHeap(&dsvHeapDesc,
		IID_PPV_ARGS(m_dsvHeap.GetAddressOf())));
}

void DX12SwapChain::ReSize(ID3D12GraphicsCommandList* cmd, unsigned int x, unsigned int y)
{
	assert(m_swapChain);

	m_renderTargets.clear();
	m_renderTargets.resize(static_cast<size_t>(m_numSwapBuffer));
	m_depthStencil = nullptr;

	CreateResources(x, y);
	CreateResourceViews();

	auto dsBarrier = CD3DX12_RESOURCE_BARRIER::Transition(m_depthStencil.Get(),
		D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_DEPTH_WRITE);
	cmd->ResourceBarrier(1, &dsBarrier);
}

void DX12SwapChain::RenderBegin(ID3D12GraphicsCommandList* cmd, const float clearColor[4])
{
	auto present = CurrRTV();

	cmd->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_renderTargets[static_cast<size_t>(m_currBackBufferIndex)].Get(),
		D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

	cmd->ClearRenderTargetView(present, clearColor, 0, nullptr);
}

void DX12SwapChain::RenderEnd(ID3D12GraphicsCommandList* cmd)
{
	CD3DX12_RESOURCE_BARRIER barrier = {};
	barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_renderTargets[static_cast<size_t>(m_currBackBufferIndex)].Get(),
		D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);

	cmd->ResourceBarrier(1, &barrier);
}

void DX12SwapChain::Present()
{
	ThrowIfFailed(m_swapChain->Present(0, 0));
	m_currBackBufferIndex = (m_currBackBufferIndex + 1) % m_numSwapBuffer;
}

D3D12_CPU_DESCRIPTOR_HANDLE DX12SwapChain::CurrRTV() const
{
	D3D12_CPU_DESCRIPTOR_HANDLE handle = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();
	handle.ptr += size_t(m_rtvDescriptorSize) * m_currBackBufferIndex;

	return handle;
}

D3D12_CPU_DESCRIPTOR_HANDLE DX12SwapChain::GetDSV() const
{
	return m_dsvHeap->GetCPUDescriptorHandleForHeapStart();
}

void DX12SwapChain::CreateResources(unsigned int x, unsigned int y)
{
	m_clientHeight = y;
	m_clientWidth = x;

	ThrowIfFailed(m_swapChain->ResizeBuffers(m_numSwapBuffer,
		x, y, m_presentBufferFormat, DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH));
	m_currBackBufferIndex = 0;

	D3D12_RESOURCE_DESC dsDesc;
	dsDesc.Alignment = 0;
	dsDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	dsDesc.Format = DXGI_FORMAT_R24G8_TYPELESS;
	dsDesc.DepthOrArraySize = 1;
	dsDesc.MipLevels = 1;
	dsDesc.Width = x;
	dsDesc.Height = y;
	dsDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
	dsDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	dsDesc.SampleDesc.Count = 1;
	dsDesc.SampleDesc.Quality = 0;

	for (size_t i = 0; i < m_numSwapBuffer; i++)
	{
		ThrowIfFailed(m_swapChain->GetBuffer(CGH::SizeTTransUINT(i), IID_PPV_ARGS(m_renderTargets[i].GetAddressOf())));
	}

	CD3DX12_HEAP_PROPERTIES heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

	D3D12_CLEAR_VALUE clearValue = {};
	memcpy(clearValue.Color, m_zero, sizeof(m_zero));
	clearValue.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	clearValue.DepthStencil.Depth = 1.0f;
	clearValue.DepthStencil.Stencil = 0;

	ThrowIfFailed(m_device->CreateCommittedResource(
		&heapProperties, D3D12_HEAP_FLAG_NONE, &dsDesc,
		D3D12_RESOURCE_STATE_COMMON, &clearValue, IID_PPV_ARGS(m_depthStencil.GetAddressOf())));
}


void DX12SwapChain::CreateResourceViews()
{
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart());
	CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(m_dsvHeap->GetCPUDescriptorHandleForHeapStart());

	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
	rtvDesc.Texture2D.MipSlice = 0;
	rtvDesc.Texture2D.PlaneSlice = 0;

	for (size_t i = 0; i < m_numSwapBuffer; i++)
	{
		rtvDesc.Format = m_presentBufferFormat;
		m_device->CreateRenderTargetView(m_renderTargets[i].Get(), &rtvDesc, rtvHandle);
		rtvHandle.Offset(1, m_rtvDescriptorSize);
	}

	//Fill dsvHeap.
	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
	dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	dsvDesc.Texture2D.MipSlice = 0;

	m_device->CreateDepthStencilView(m_depthStencil.Get(), &dsvDesc, dsvHandle);
}

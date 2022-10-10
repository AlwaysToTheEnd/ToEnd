#include "d3dx12.h"
#include "DX12SwapChain.h"
#include "CGHUtil.h"
#include "DxException.h"

using Microsoft::WRL::ComPtr;

DX12SwapChain::DX12SwapChain()
	: m_Device(nullptr)
	, m_ColorBufferFormat(DXGI_FORMAT_UNKNOWN)
	, m_PresentBufferFormat(DXGI_FORMAT_UNKNOWN)
	, m_NumSwapBuffer(2)
	, m_CurrBackBufferIndex(0)
	, m_RTVDescriptorSize(0)
	, m_DSVDescriptorSize(0)
	, m_NormalBufferFormat(DXGI_FORMAT_UNKNOWN)
	, m_SRVDescriptorSize(0)
	, m_SpecPowBufferFormat(DXGI_FORMAT_UNKNOWN)
	, m_ObjectIDFormat(DXGI_FORMAT_UNKNOWN)
	, m_ClientWidth(0)
	, m_ClientHeight(0)
	, m_PixelFuncReadBackRowPitch(0)
{
}

DX12SwapChain::~DX12SwapChain()
{


}

void DX12SwapChain::CreateDXGIFactory(ID3D12Device** device)
{
	ThrowIfFailed(CreateDXGIFactory1(IID_PPV_ARGS(m_DxgiFactory.GetAddressOf())));

	if (*device == nullptr)
	{
		ComPtr<IDXGIAdapter> pWarpAdapter;
		ThrowIfFailed(m_DxgiFactory->EnumWarpAdapter(IID_PPV_ARGS(pWarpAdapter.GetAddressOf())));

		ThrowIfFailed(D3D12CreateDevice(pWarpAdapter.Get(), D3D_FEATURE_LEVEL_12_1, IID_PPV_ARGS(device)));
	}

	m_Device = *device;

	m_RTVDescriptorSize = m_Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	m_DSVDescriptorSize = m_Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
	m_SRVDescriptorSize = m_Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
}

void DX12SwapChain::CreateSwapChain(HWND handle, ID3D12CommandQueue* queue, DXGI_FORMAT renderTarget,
	unsigned int x, unsigned int y, unsigned int numSwapBuffer)
{
	m_NumSwapBuffer = numSwapBuffer;
	m_PresentBufferFormat = renderTarget;
	m_ColorBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
	m_NormalBufferFormat = DXGI_FORMAT_R11G11B10_FLOAT;
	m_SpecPowBufferFormat = DXGI_FORMAT_R32_FLOAT;
	m_ObjectIDFormat = DXGI_FORMAT_R32_SINT;

	DXGI_SWAP_CHAIN_DESC sd;
	sd.BufferDesc.Width = x;
	sd.BufferDesc.Height = y;
	sd.BufferDesc.RefreshRate.Numerator = 60;
	sd.BufferDesc.RefreshRate.Denominator = 1;
	sd.BufferDesc.Format = m_PresentBufferFormat;
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

	ThrowIfFailed(m_DxgiFactory->CreateSwapChain(queue, &sd, m_SwapChain.GetAddressOf()));

	m_Resources.resize(GBUFFER_RESOURCE_PRESENT + static_cast<size_t>(m_NumSwapBuffer));

	D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc;
	rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	rtvHeapDesc.NodeMask = 0;
	rtvHeapDesc.NumDescriptors = CGH::SizeTTransUINT(m_Resources.size() - 1);
	rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;

	D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc;
	dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	dsvHeapDesc.NodeMask = 0;
	dsvHeapDesc.NumDescriptors = 1;
	dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;

	D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc;
	srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	srvHeapDesc.NodeMask = 0;
	srvHeapDesc.NumDescriptors = GBUFFER_RESOURCE_PRESENT;
	srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;

	ThrowIfFailed(m_Device->CreateDescriptorHeap(&rtvHeapDesc,
		IID_PPV_ARGS(m_RTVHeap.GetAddressOf())));
	ThrowIfFailed(m_Device->CreateDescriptorHeap(&dsvHeapDesc,
		IID_PPV_ARGS(m_DSVHeap.GetAddressOf())));
	ThrowIfFailed(m_Device->CreateDescriptorHeap(&srvHeapDesc,
		IID_PPV_ARGS(m_SRVHeap.GetAddressOf())));
}

void DX12SwapChain::ReSize(ID3D12GraphicsCommandList* cmd, unsigned int x, unsigned int y)
{
	assert(m_SwapChain);

	m_Resources.clear();
	m_Resources.resize(GBUFFER_RESOURCE_PRESENT + static_cast<size_t>(m_NumSwapBuffer));

	m_PixelFuncReadBack = nullptr;

	CreateResources(x, y);
	CreateResourceViews();

	auto dsBarrier = CD3DX12_RESOURCE_BARRIER::Transition(m_Resources[GBUFFER_RESOURCE_DS].Get(),
		D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_DEPTH_WRITE);
	cmd->ResourceBarrier(1, &dsBarrier);
}

void DX12SwapChain::GbufferSetting(ID3D12GraphicsCommandList* cmd)
{
	auto depthStencil = GetDSV();
	auto color = CurrRTV(GBUFFER_RESOURCE_COLORS);
	auto normal = CurrRTV(GBUFFER_RESOURCE_NORMAL);
	auto specular = CurrRTV(GBUFFER_RESOURCE_SPECPOWER);
	auto objectID = CurrRTV(GBUFFER_RESOURCE_OBJECTID);

	D3D12_CPU_DESCRIPTOR_HANDLE renderTargetHandles[] =
	{
		color,
		normal,
		specular,
		objectID
	};

	cmd->ClearDepthStencilView(depthStencil, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

	cmd->OMSetRenderTargets(_countof(renderTargetHandles), renderTargetHandles, false, &depthStencil);

	cmd->ClearRenderTargetView(color, m_Zero, 0, nullptr);
	cmd->ClearRenderTargetView(normal, m_Zero, 0, nullptr);
	cmd->ClearRenderTargetView(specular, m_Zero, 0, nullptr);
	cmd->ClearRenderTargetView(objectID, m_MinorIntiger, 0, nullptr);
}

void DX12SwapChain::PresentRenderTargetSetting(ID3D12GraphicsCommandList* cmd, const float clearColor[4])
{
	auto present = CurrRTV(GBUFFER_RESOURCE_PRESENT);

	D3D12_CPU_DESCRIPTOR_HANDLE renderTargetHandles[] =
	{
		present,
	};

	CD3DX12_RESOURCE_BARRIER renderTargetBarrier = CD3DX12_RESOURCE_BARRIER::Transition(m_Resources[GBUFFER_RESOURCE_PRESENT + static_cast<size_t>(m_CurrBackBufferIndex)].Get(),
		D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
	cmd->ResourceBarrier(1, &renderTargetBarrier);

	cmd->ClearRenderTargetView(present, clearColor, 0, nullptr);

	cmd->OMSetRenderTargets(_countof(renderTargetHandles), renderTargetHandles, false, nullptr);
}

void DX12SwapChain::RenderEnd(ID3D12GraphicsCommandList* cmd)
{
	CD3DX12_RESOURCE_BARRIER barriers[3] = {};
	barriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(m_Resources[GBUFFER_RESOURCE_PRESENT + static_cast<size_t>(m_CurrBackBufferIndex)].Get(),
		D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
	barriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(m_Resources[GBUFFER_RESOURCE_OBJECTID].Get(),
		D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_SOURCE);
	barriers[2] = CD3DX12_RESOURCE_BARRIER::Transition(m_Resources[GBUFFER_RESOURCE_OBJECTID].Get(),
		D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);

	cmd->ResourceBarrier(2, barriers);

	D3D12_TEXTURE_COPY_LOCATION src = {};
	src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
	src.pResource = m_Resources[GBUFFER_RESOURCE_OBJECTID].Get();
	src.SubresourceIndex = 0;

	D3D12_TEXTURE_COPY_LOCATION dst = {};
	D3D12_PLACED_SUBRESOURCE_FOOTPRINT dstFoot = {};
	dst.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
	dst.pResource = m_PixelFuncReadBack.Get();

	dstFoot.Offset = 0;
	dstFoot.Footprint.Format = m_ObjectIDFormat;
	dstFoot.Footprint.Depth = 1;
	dstFoot.Footprint.Width = m_ClientWidth;
	dstFoot.Footprint.Height = m_ClientHeight;
	dstFoot.Footprint.RowPitch = CGH::SizeTTransUINT(m_PixelFuncReadBackRowPitch);

	dst.PlacedFootprint = dstFoot;

	cmd->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
	cmd->ResourceBarrier(1, &barriers[2]);
}

void DX12SwapChain::Present()
{
	ThrowIfFailed(m_SwapChain->Present(0, 0));
	m_CurrBackBufferIndex = (m_CurrBackBufferIndex + 1) % m_NumSwapBuffer;
}

int DX12SwapChain::GetPixelFuncMapData(UINT x, UINT y)
{
	int result = -1;
	BYTE* readBackData = nullptr;

	ThrowIfFailed(m_PixelFuncReadBack->Map(0, nullptr, reinterpret_cast<void**>(&readBackData)));

	readBackData += (x * sizeof(int)) + (y * m_PixelFuncReadBackRowPitch);
	memcpy(&result, readBackData, sizeof(int));

	m_PixelFuncReadBack->Unmap(0, nullptr);

	return result;
}

D3D12_CPU_DESCRIPTOR_HANDLE DX12SwapChain::CurrRTV(BUFFER_RESURECE_TYPE type) const
{
	D3D12_CPU_DESCRIPTOR_HANDLE resultHandle = m_RTVHeap->GetCPUDescriptorHandleForHeapStart();

	switch (type)
	{
	case DX12SwapChain::GBUFFER_RESOURCE_COLORS:
	case DX12SwapChain::GBUFFER_RESOURCE_NORMAL:
	case DX12SwapChain::GBUFFER_RESOURCE_SPECPOWER:
	case DX12SwapChain::GBUFFER_RESOURCE_OBJECTID:
	{
		resultHandle.ptr += m_RTVDescriptorSize * static_cast<size_t>(type);
	}
	break;
	case DX12SwapChain::GBUFFER_RESOURCE_PRESENT:
	{
		resultHandle.ptr += m_RTVDescriptorSize * (static_cast<size_t>(m_CurrBackBufferIndex) + GBUFFER_RESOURCE_PRESENT - 1);
	}
	break;
	default:
		assert("nonType");
		break;
	}

	return resultHandle;
}

D3D12_CPU_DESCRIPTOR_HANDLE DX12SwapChain::GetDSV() const
{
	return m_DSVHeap->GetCPUDescriptorHandleForHeapStart();
}

D3D12_GPU_DESCRIPTOR_HANDLE DX12SwapChain::GetSRVsGPU() const
{
	return m_SRVHeap->GetGPUDescriptorHandleForHeapStart();
}

void DX12SwapChain::CreateResources(unsigned int x, unsigned int y)
{
	m_ClientHeight = y;
	m_ClientWidth = x;

	ThrowIfFailed(m_SwapChain->ResizeBuffers(m_NumSwapBuffer,
		x, y, m_PresentBufferFormat, DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH));
	m_CurrBackBufferIndex = 0;

	D3D12_RESOURCE_DESC colorDesc = {};
	colorDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	colorDesc.Format = m_ColorBufferFormat;
	colorDesc.DepthOrArraySize = 1;
	colorDesc.MipLevels = 1;
	colorDesc.Width = x;
	colorDesc.Height = y;
	colorDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
	colorDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	colorDesc.SampleDesc.Count = 1;
	colorDesc.SampleDesc.Quality = 0;

	D3D12_RESOURCE_DESC normalDesc = {};
	normalDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	normalDesc.Format = m_NormalBufferFormat;
	normalDesc.DepthOrArraySize = 1;
	normalDesc.MipLevels = 1;
	normalDesc.Width = x;
	normalDesc.Height = y;
	normalDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
	normalDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	normalDesc.SampleDesc.Count = 1;
	normalDesc.SampleDesc.Quality = 0;

	D3D12_RESOURCE_DESC objectIDDesc = {};
	objectIDDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	objectIDDesc.Format = m_ObjectIDFormat;
	objectIDDesc.DepthOrArraySize = 1;
	objectIDDesc.MipLevels = 1;
	objectIDDesc.Width = x;
	objectIDDesc.Height = y;
	objectIDDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
	objectIDDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	objectIDDesc.SampleDesc.Count = 1;
	objectIDDesc.SampleDesc.Quality = 0;

	D3D12_RESOURCE_DESC specularDesc = {};
	specularDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	specularDesc.Format = m_SpecPowBufferFormat;
	specularDesc.DepthOrArraySize = 1;
	specularDesc.MipLevels = 1;
	specularDesc.Width = x;
	specularDesc.Height = y;
	specularDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
	specularDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	specularDesc.SampleDesc.Count = 1;
	specularDesc.SampleDesc.Quality = 0;

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

	m_PixelFuncReadBackRowPitch = (static_cast<UINT64>(x) * sizeof(int) + 255) & ~255;

	D3D12_RESOURCE_DESC texFuncDesc = {};
	texFuncDesc.Alignment = 0;
	texFuncDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	texFuncDesc.Format = DXGI_FORMAT_UNKNOWN;
	texFuncDesc.DepthOrArraySize = 1;
	texFuncDesc.MipLevels = 1;
	texFuncDesc.Width = m_PixelFuncReadBackRowPitch * y;
	texFuncDesc.Height = 1;
	texFuncDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
	texFuncDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	texFuncDesc.SampleDesc.Count = 1;
	texFuncDesc.SampleDesc.Quality = 0;

	for (size_t i = 0; i < m_NumSwapBuffer; i++)
	{
		ThrowIfFailed(m_SwapChain->GetBuffer(CGH::SizeTTransUINT(i), IID_PPV_ARGS(m_Resources[GBUFFER_RESOURCE_PRESENT + i].GetAddressOf())));
	}

	D3D12_CLEAR_VALUE clearValue = {};

	clearValue.Format = colorDesc.Format;
	memcpy(clearValue.Color, m_Zero, sizeof(m_Zero));

	CD3DX12_HEAP_PROPERTIES heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

	ThrowIfFailed(m_Device->CreateCommittedResource(
		&heapProperties, D3D12_HEAP_FLAG_NONE, &colorDesc,
		D3D12_RESOURCE_STATE_RENDER_TARGET, &clearValue, IID_PPV_ARGS(m_Resources[GBUFFER_RESOURCE_COLORS].GetAddressOf())));

	clearValue.Format = normalDesc.Format;

	ThrowIfFailed(m_Device->CreateCommittedResource(
		&heapProperties, D3D12_HEAP_FLAG_NONE, &normalDesc,
		D3D12_RESOURCE_STATE_RENDER_TARGET, &clearValue, IID_PPV_ARGS(m_Resources[GBUFFER_RESOURCE_NORMAL].GetAddressOf())));

	clearValue.Format = specularDesc.Format;

	ThrowIfFailed(m_Device->CreateCommittedResource(
		&heapProperties, D3D12_HEAP_FLAG_NONE, &specularDesc,
		D3D12_RESOURCE_STATE_RENDER_TARGET, &clearValue, IID_PPV_ARGS(m_Resources[GBUFFER_RESOURCE_SPECPOWER].GetAddressOf())));

	clearValue.Format = objectIDDesc.Format;
	memcpy(clearValue.Color, m_MinorIntiger, sizeof(m_MinorIntiger));

	ThrowIfFailed(m_Device->CreateCommittedResource(
		&heapProperties, D3D12_HEAP_FLAG_NONE, &objectIDDesc,
		D3D12_RESOURCE_STATE_RENDER_TARGET, &clearValue, IID_PPV_ARGS(m_Resources[GBUFFER_RESOURCE_OBJECTID].GetAddressOf())));

	clearValue.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	clearValue.DepthStencil.Depth = 1.0f;
	clearValue.DepthStencil.Stencil = 0;

	ThrowIfFailed(m_Device->CreateCommittedResource(
		&heapProperties, D3D12_HEAP_FLAG_NONE, &dsDesc,
		D3D12_RESOURCE_STATE_COMMON, &clearValue, IID_PPV_ARGS(m_Resources[GBUFFER_RESOURCE_DS].GetAddressOf())));

	heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK);

	ThrowIfFailed(m_Device->CreateCommittedResource(
		&heapProperties, D3D12_HEAP_FLAG_NONE, &texFuncDesc,
		D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(m_PixelFuncReadBack.GetAddressOf())));
}


void DX12SwapChain::CreateResourceViews()
{
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_RTVHeap->GetCPUDescriptorHandleForHeapStart());
	CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(m_DSVHeap->GetCPUDescriptorHandleForHeapStart());
	CD3DX12_CPU_DESCRIPTOR_HANDLE srvHandle(m_SRVHeap->GetCPUDescriptorHandleForHeapStart());

	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
	rtvDesc.Texture2D.MipSlice = 0;
	rtvDesc.Texture2D.PlaneSlice = 0;

	//Fill rtvHeap.
	rtvDesc.Format = m_ColorBufferFormat;
	m_Device->CreateRenderTargetView(m_Resources[GBUFFER_RESOURCE_COLORS].Get(), &rtvDesc, rtvHandle);
	rtvHandle.Offset(1, m_RTVDescriptorSize);

	rtvDesc.Format = m_NormalBufferFormat;
	m_Device->CreateRenderTargetView(m_Resources[GBUFFER_RESOURCE_NORMAL].Get(), &rtvDesc, rtvHandle);
	rtvHandle.Offset(1, m_RTVDescriptorSize);

	rtvDesc.Format = m_SpecPowBufferFormat;
	m_Device->CreateRenderTargetView(m_Resources[GBUFFER_RESOURCE_SPECPOWER].Get(), &rtvDesc, rtvHandle);
	rtvHandle.Offset(1, m_RTVDescriptorSize);

	rtvDesc.Format = m_ObjectIDFormat;
	m_Device->CreateRenderTargetView(m_Resources[GBUFFER_RESOURCE_OBJECTID].Get(), &rtvDesc, rtvHandle);
	rtvHandle.Offset(1, m_RTVDescriptorSize);

	for (size_t i = 0; i < m_NumSwapBuffer; i++)
	{
		rtvDesc.Format = m_PresentBufferFormat;
		m_Device->CreateRenderTargetView(m_Resources[GBUFFER_RESOURCE_PRESENT + i].Get(), &rtvDesc, rtvHandle);
		rtvHandle.Offset(1, m_RTVDescriptorSize);
	}

	//Fill dsvHeap.
	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
	dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	dsvDesc.Texture2D.MipSlice = 0;

	m_Device->CreateDepthStencilView(m_Resources[GBUFFER_RESOURCE_DS].Get(), &dsvDesc, dsvHandle);
	dsvHandle.Offset(1, m_DSVDescriptorSize);

	//Fill srvHeap.
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};

	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

	srvDesc.Format = m_ColorBufferFormat;
	m_Device->CreateShaderResourceView(m_Resources[GBUFFER_RESOURCE_COLORS].Get(), &srvDesc, srvHandle);
	srvHandle.Offset(1, m_SRVDescriptorSize);

	srvDesc.Format = m_NormalBufferFormat;
	m_Device->CreateShaderResourceView(m_Resources[GBUFFER_RESOURCE_NORMAL].Get(), &srvDesc, srvHandle);
	srvHandle.Offset(1, m_SRVDescriptorSize);

	srvDesc.Format = m_SpecPowBufferFormat;
	m_Device->CreateShaderResourceView(m_Resources[GBUFFER_RESOURCE_SPECPOWER].Get(), &srvDesc, srvHandle);
	srvHandle.Offset(1, m_SRVDescriptorSize);

	srvDesc.Format = m_ObjectIDFormat;
	m_Device->CreateShaderResourceView(m_Resources[GBUFFER_RESOURCE_OBJECTID].Get(), &srvDesc, srvHandle);
	srvHandle.Offset(1, m_SRVDescriptorSize);

	srvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
	m_Device->CreateShaderResourceView(m_Resources[GBUFFER_RESOURCE_DS].Get(), &srvDesc, srvHandle);
}

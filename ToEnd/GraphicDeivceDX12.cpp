#include "GraphicDeivceDX12.h"
#include "DxException.h"
#include "CGHUtil.h"

GraphicDeviceDX12* GraphicDeviceDX12::s_Graphic = nullptr;

GraphicDeviceDX12* GraphicDeviceDX12::GetGraphic()
{
	return s_Graphic;
}

ID3D12Device* GraphicDeviceDX12::GetDevice()
{
	return s_Graphic->m_d3dDevice.Get();
}

void GraphicDeviceDX12::CreateDeivce(HWND hWnd, int windowWidth, int windowHeight)
{
	if (s_Graphic == nullptr)
	{
		s_Graphic = new GraphicDeviceDX12;
		s_Graphic->Init(hWnd, windowWidth, windowHeight);
	}
}

void GraphicDeviceDX12::DeleteDeivce()
{
	if (s_Graphic)
	{
		delete s_Graphic;
		s_Graphic = nullptr;
	}
}

void GraphicDeviceDX12::Init(HWND hWnd, int windowWidth, int windowHeight)
{
#if defined(DEBUG)||defined(_DEBUG)
	{
		ComPtr<ID3D12Debug> debugController;
		ThrowIfFailed(D3D12GetDebugInterface(IID_PPV_ARGS(debugController.GetAddressOf())));
		debugController->EnableDebugLayer();
	}
#endif

	HRESULT hr = S_OK;

	hr = D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_12_1, IID_PPV_ARGS(m_d3dDevice.GetAddressOf()));

	ThrowIfFailed(CreateDXGIFactory1(IID_PPV_ARGS(m_dxgiFactory.GetAddressOf())));

	// Fallback to WARP device.
	if (FAILED(hr))
	{
		ComPtr<IDXGIAdapter> pWarpAdapter;
		ThrowIfFailed(m_dxgiFactory->EnumWarpAdapter(IID_PPV_ARGS(&pWarpAdapter)));

		ThrowIfFailed(D3D12CreateDevice(
			pWarpAdapter.Get(),
			D3D_FEATURE_LEVEL_11_0,
			IID_PPV_ARGS(m_d3dDevice.GetAddressOf())));
	}

	{
		D3D12_COMMAND_QUEUE_DESC queueDesc = {};
		queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
		queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

		ThrowIfFailed(m_d3dDevice->CreateCommandQueue(&queueDesc,
			IID_PPV_ARGS(m_commandQueue.GetAddressOf())));

		ThrowIfFailed(m_d3dDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
			IID_PPV_ARGS(m_directCmdListAlloc.GetAddressOf())));

		ThrowIfFailed(m_d3dDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
			m_directCmdListAlloc.Get(), nullptr, IID_PPV_ARGS(m_commandList.GetAddressOf())));

		m_commandList->Close();

		ThrowIfFailed(m_d3dDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE,
			IID_PPV_ARGS(m_fence.GetAddressOf())));
	}

	{
		DXGI_SWAP_CHAIN_DESC sd;
		sd.BufferDesc.Width = windowWidth;
		sd.BufferDesc.Height = windowHeight;
		sd.BufferDesc.RefreshRate.Numerator = 60;
		sd.BufferDesc.RefreshRate.Denominator = 1;
		sd.BufferDesc.Format = m_backBufferFormat;
		sd.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UPPER_FIELD_FIRST;
		sd.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
		sd.SampleDesc.Count = 1;
		sd.SampleDesc.Quality = 0;
		sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		sd.BufferCount = m_numBackBuffer;
		sd.OutputWindow = hWnd;
		sd.Windowed = true;
		sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

		ThrowIfFailed(m_dxgiFactory->CreateSwapChain(m_commandQueue.Get(), &sd, m_swapChain.GetAddressOf()));

		m_backBufferResource.resize(sd.BufferCount);
	}

	OnResize(windowWidth , windowHeight);

	m_cmdListAllocs.resize(m_numFrameResource);
	m_cmdLists.resize(m_numFrameResource);

	for (UINT i = 0; i < m_numFrameResource; i++)
	{
		ThrowIfFailed(m_d3dDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
			IID_PPV_ARGS(m_cmdListAllocs[i].GetAddressOf())));

		ThrowIfFailed(m_d3dDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
			m_cmdListAllocs[i].Get(), nullptr, IID_PPV_ARGS(m_cmdLists[i].GetAddressOf())));
	}
}

void GraphicDeviceDX12::OnResize(int windowWidth, int windowHeight)
{
	FlushCommandQueue();

	m_screenViewport.TopLeftX = 0;
	m_screenViewport.TopLeftY = 0;
	m_screenViewport.Width = static_cast<float>(windowWidth);
	m_screenViewport.Height = static_cast<float>(windowHeight);
	m_screenViewport.MinDepth = 0.0f;
	m_screenViewport.MaxDepth = 1.0f;

	m_scissorRect = { 0, 0, windowWidth, windowHeight };

	ThrowIfFailed(m_directCmdListAlloc->Reset());
	ThrowIfFailed(m_commandList->Reset(m_directCmdListAlloc.Get(), nullptr));
	{
		m_backBufferRtvHeap = nullptr;
		m_currBackBufferIndex = 0;

		for (size_t i = 0; i < m_backBufferResource.size(); i++)
		{
			m_backBufferResource[i] = nullptr;
		}

		ThrowIfFailed(m_swapChain->ResizeBuffers(m_numBackBuffer,
			windowWidth, windowHeight, m_backBufferFormat, DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH));

		D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
		heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		heapDesc.NumDescriptors = m_numBackBuffer;

		ThrowIfFailed(m_d3dDevice->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(m_backBufferRtvHeap.GetAddressOf())));

		D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_backBufferRtvHeap->GetCPUDescriptorHandleForHeapStart();
		D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
		UINT rtvSize = m_d3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

		rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
		rtvDesc.Format = m_backBufferFormat;

		for (size_t i = 0; i < m_numBackBuffer; i++)
		{
			size_t currIndex = CGH::SizeTTransUINT(i);
			ThrowIfFailed(m_swapChain->GetBuffer(currIndex, IID_PPV_ARGS(m_backBufferResource[currIndex].GetAddressOf())));

			m_d3dDevice->CreateRenderTargetView(m_backBufferResource[currIndex].Get(), &rtvDesc, rtvHandle);
			rtvHandle.ptr += rtvSize;
		}
	}
	ThrowIfFailed(m_commandList->Close());
	ID3D12CommandList* cmdLists[] = { m_commandList.Get() };
	m_commandQueue->ExecuteCommandLists(_countof(cmdLists), cmdLists);
	
	FlushCommandQueue();

	ThrowIfFailed(m_directCmdListAlloc->Reset());
}

void GraphicDeviceDX12::RenderBegin()
{
}

void GraphicDeviceDX12::RenderEnd()
{
	auto cmdListAlloc = m_cmdListAllocs[m_currFrame].Get();

	ThrowIfFailed(cmdListAlloc->Reset());
	ThrowIfFailed(m_commandList->Reset(cmdListAlloc, nullptr));

	m_commandList->RSSetViewports(1, &m_screenViewport);
	m_commandList->RSSetScissorRects(1, &m_scissorRect);

	ThrowIfFailed(m_commandList->Close());
	ID3D12CommandList* cmdsLists[] = { m_commandList.Get() };
	m_commandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	FlushCommandQueue();

	m_currFrame = (m_currFrame + 1) % m_numFrameResource;
}

void GraphicDeviceDX12::FlushCommandQueue()
{
	m_currentFence++;

	ThrowIfFailed(m_commandQueue->Signal(m_fence.Get(), m_currentFence));

	if (m_fence->GetCompletedValue() < m_currentFence)
	{
		HANDLE eventHandle = CreateEventEx(nullptr, nullptr, false, EVENT_ALL_ACCESS);

		assert(false);

		WaitForSingleObject(eventHandle, INFINITE);
	}
}

#include "GraphicDeivceDX12.h"
#include "../Common/Source/CGHUtil.h"
#include "../Common/Source/DxException.h"
#include "../Common/Source/DX12SwapChain.h"

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
		delete s_Graphic->m_swapChain;
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

	if (m_swapChain)
	{
		delete m_swapChain;
		m_swapChain = nullptr;
	}

	m_fenceCounts.resize(m_numFrameResource);
	m_swapChain = new DX12SwapChain;
	m_swapChain->CreateDXGIFactory(m_d3dDevice.GetAddressOf());

	{
		D3D12_COMMAND_QUEUE_DESC queueDesc = {};
		queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
		queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

		ThrowIfFailed(m_d3dDevice->CreateCommandQueue(&queueDesc,
			IID_PPV_ARGS(m_commandQueue.GetAddressOf())));

		m_cmdLists.resize(m_numFrameResource);
		m_cmdListAllocs.resize(m_numFrameResource);
		for (size_t i = 0; i < m_numFrameResource; i++)
		{
			ThrowIfFailed(m_d3dDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
				IID_PPV_ARGS(m_cmdListAllocs[i].GetAddressOf())));

			ThrowIfFailed(m_d3dDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
				m_cmdListAllocs[i].Get(), nullptr, IID_PPV_ARGS(m_cmdLists[i].GetAddressOf())));

			m_cmdLists[i]->Close();
		}

		ThrowIfFailed(m_d3dDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE,
			IID_PPV_ARGS(m_fence.GetAddressOf())));
	}

	m_swapChain->CreateSwapChain(hWnd, m_commandQueue.Get(), m_backBufferFormat, windowWidth, windowHeight, 2);

	OnResize(windowWidth, windowHeight);
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

	auto cmdListAlloc = GetCurrCommandAllocator();
	auto cmdList = GetCurrCommandList();

	ThrowIfFailed(cmdList->Reset(cmdListAlloc, nullptr));
	{
		m_swapChain->ReSize(cmdList, windowWidth, windowHeight);
	}
	ThrowIfFailed(cmdList->Close());

	ID3D12CommandList* cmdLists[] = { cmdList };
	m_commandQueue->ExecuteCommandLists(_countof(cmdLists), cmdLists);

	FlushCommandQueue();

	ThrowIfFailed(cmdListAlloc->Reset());
}

void GraphicDeviceDX12::RenderBegin()
{
	if (m_fence->GetCompletedValue() < m_fenceCounts[m_currFrame])
	{
		HANDLE eventHandle = CreateEventEx(nullptr, nullptr, false, EVENT_ALL_ACCESS);
		ThrowIfFailed(m_fence->SetEventOnCompletion(m_fenceCounts[m_currFrame], eventHandle));

		if (eventHandle)
		{
			WaitForSingleObject(eventHandle, INFINITE);
			CloseHandle(eventHandle);
		}
		else
		{
			assert(false);
		}
	}

	auto cmdListAlloc = GetCurrCommandAllocator();
	auto cmdList = GetCurrCommandList();

	ThrowIfFailed(cmdListAlloc->Reset());
	ThrowIfFailed(cmdList->Reset(cmdListAlloc, nullptr));

	cmdList->RSSetViewports(1, &m_screenViewport);
	cmdList->RSSetScissorRects(1, &m_scissorRect);

	m_swapChain->GbufferSetting(cmdList);
}

void GraphicDeviceDX12::LightRenderBegin()
{
	float backGroundColor[4] = { 0.0f,0.0f,1.0f };
	m_swapChain->PresentRenderTargetSetting(GetCurrCommandList(), backGroundColor);
}

void GraphicDeviceDX12::RenderEnd()
{
	auto cmdList = GetCurrCommandList();
	m_swapChain->RenderEnd(cmdList);

	ThrowIfFailed(cmdList->Close());
	ID3D12CommandList* cmdsLists[] = { cmdList };
	m_commandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	m_swapChain->Present();

	m_currentFence++;
	m_fenceCounts[m_currFrame] = m_currentFence;	
	ThrowIfFailed(m_commandQueue->Signal(m_fence.Get(), m_currentFence));

	m_currFrame = (m_currFrame + 1) % m_numFrameResource;
}

void GraphicDeviceDX12::FlushCommandQueue()
{
	m_currentFence++;

	ThrowIfFailed(m_commandQueue->Signal(m_fence.Get(), m_currentFence));

	if (m_fence->GetCompletedValue() < m_currentFence)
	{
		HANDLE eventHandle = CreateEventEx(nullptr, nullptr, false, EVENT_ALL_ACCESS);
		ThrowIfFailed(m_fence->SetEventOnCompletion(m_currentFence, eventHandle));

		if (eventHandle)
		{
			WaitForSingleObject(eventHandle, INFINITE);
			CloseHandle(eventHandle);
		}
		else
		{
			assert(false);
		}
	}
}

ID3D12GraphicsCommandList* GraphicDeviceDX12::GetCurrCommandList()
{
	return m_cmdLists[m_currFrame].Get();
}

ID3D12CommandAllocator* GraphicDeviceDX12::GetCurrCommandAllocator()
{
	return m_cmdListAllocs[m_currFrame].Get();
}

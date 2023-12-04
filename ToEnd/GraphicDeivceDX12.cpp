#include "GraphicDeivceDX12.h"
#include "../Common/Source/CGHUtil.h"
#include "../Common/Source/DxException.h"
#include "../Common/Source/DX12SwapChain.h"

#include "GraphicResourceLoader.h"
#include "Camera.h"

using namespace DirectX;
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
	m_passCBs.resize(m_numFrameResource);

	for (size_t i = 0; i < m_passCBs.size(); i++)
	{
		m_passCBs[i] = std::make_unique<DX12UploadBuffer<DX12PassConstants>>(m_d3dDevice.Get(), 1, true);
	}

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

	DX12GraphicResourceLoader loader;
	CGHMeshDataSet meshDataset;
	CGHMaterialSet materialSet;
	std::vector<ComPtr<ID3D12Resource>> upBuffers;

	m_cmdLists.front()->Reset(m_cmdListAllocs[0].Get(), nullptr);
	loader.LoadAllData("./../Common/MeshData/pants.fbx", aiComponent_CAMERAS | aiComponent_TEXTURES | aiComponent_COLORS | aiComponent_LIGHTS, m_cmdLists.front().Get(), 
		meshDataset, materialSet, upBuffers);

	m_cmdLists.front()->Close();

	ID3D12CommandList* cmdLists[2] = { m_cmdLists.front().Get(), nullptr };
	m_commandQueue->ExecuteCommandLists(1, cmdLists);

	FlushCommandQueue();

}

void GraphicDeviceDX12::Update(float delta, const Camera* camera)
{
	assert(camera);

	DX12PassConstants passCons;

	XMVECTOR deter;
	XMMATRIX xmView = XMLoadFloat4x4(camera->GetViewMatrix());
	XMMATRIX xmProj = XMLoadFloat4x4(&m_projMat);
	XMMATRIX xmViewProj = XMMatrixMultiply(xmView, xmProj);
	XMMATRIX xmInvView = XMMatrixInverse(&deter, xmView);

	XMStoreFloat4x4(&passCons.view, XMMatrixTranspose(xmView));
	XMStoreFloat4x4(&passCons.proj, XMMatrixTranspose(xmProj));
	XMStoreFloat4x4(&passCons.viewProj, XMMatrixTranspose(xmViewProj));
	XMStoreFloat4x4(&passCons.invView, XMMatrixTranspose(xmInvView));
	XMStoreFloat4x4(&passCons.invProj, XMMatrixTranspose(XMMatrixInverse(&deter, xmProj)));
	XMStoreFloat4x4(&passCons.invViewProj, XMMatrixTranspose(XMMatrixInverse(&deter, xmViewProj)));

	passCons.renderTargetSizeX = m_scissorRect.right;
	passCons.renderTargetSizeY = m_scissorRect.bottom;
	passCons.invRenderTargetSize = { 1.0f / m_screenViewport.Width, 1.0f / m_screenViewport.Height };
	//passCons.samplerIndex = 2;
	passCons.eyePosW = camera->GetEyePos();
	passCons.ambientLight = { 0.25f, 0.25f, 0.35f, 1.0f }; //TODO ambientLight
	passCons.mousePos = camera->GetMousePos();

	m_passCBs[m_currFrame]->CopyData(0, &passCons);

	XMVECTOR rayOrigin = XMVectorZero();
	XMVECTOR ray = XMLoadFloat3(&camera->GetViewRay(m_projMat, static_cast<unsigned int>(m_screenViewport.Width), static_cast<unsigned int>(m_screenViewport.Height)));
	
	rayOrigin = XMVector3Transform(rayOrigin, xmInvView);
	ray = (xmInvView.r[0] * ray.m128_f32[0]) + (xmInvView.r[1] * ray.m128_f32[1]) + (xmInvView.r[2] * ray.m128_f32[2]);

	XMStoreFloat3(&m_ray, ray);
	XMStoreFloat3(&m_rayOrigin, rayOrigin);
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
	auto cmdList = GetCurrGraphicsCommandList();

	ThrowIfFailed(cmdList->Reset(cmdListAlloc, nullptr));
	{
		m_swapChain->ReSize(cmdList, windowWidth, windowHeight);
	}
	ThrowIfFailed(cmdList->Close());

	ID3D12CommandList* cmdLists[] = { cmdList };
	m_commandQueue->ExecuteCommandLists(_countof(cmdLists), cmdLists);

	float fovAngleY = 0.785398163f;
	float aspectRatio = (float)windowWidth / windowHeight;
	float fovAngleX = 2 * atanf(aspectRatio * tanf(fovAngleY));

	XMMATRIX proj = XMMatrixPerspectiveFovLH(fovAngleY, aspectRatio, 1.0f, 1000.0f);
	XMStoreFloat4x4(&m_projMat, XMMatrixTranspose(proj));

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
	auto cmdList = GetCurrGraphicsCommandList();

	ThrowIfFailed(cmdListAlloc->Reset());
	ThrowIfFailed(cmdList->Reset(cmdListAlloc, nullptr));

	cmdList->RSSetViewports(1, &m_screenViewport);
	cmdList->RSSetScissorRects(1, &m_scissorRect);

	m_swapChain->GbufferSetting(cmdList);
	m_currPipeline = nullptr;
}

void GraphicDeviceDX12::LightRenderBegin()
{
	float backGroundColor[4] = { 0.0f,0.0f,1.0f };
	m_swapChain->PresentRenderTargetSetting(GetCurrGraphicsCommandList(), backGroundColor);
}

void GraphicDeviceDX12::RenderEnd()
{
	auto cmdList = GetCurrGraphicsCommandList();
	m_swapChain->RenderEnd(cmdList);

	ThrowIfFailed(cmdList->Close());
	ID3D12CommandList* cmdsLists[] = { cmdList };
	m_commandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	m_swapChain->Present();

	m_currentFence++;
	m_fenceCounts[m_currFrame] = m_currentFence;
	ThrowIfFailed(m_commandQueue->Signal(m_fence.Get(), m_currentFence));

	m_currFrame = (m_currFrame + 1) % m_numFrameResource;

	FlushCommandQueue();
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

ID3D12GraphicsCommandList* GraphicDeviceDX12::GetCurrGraphicsCommandList()
{
	return m_cmdLists[m_currFrame].Get();
}

ID3D12CommandAllocator* GraphicDeviceDX12::GetCurrCommandAllocator()
{
	return m_cmdListAllocs[m_currFrame].Get();
}

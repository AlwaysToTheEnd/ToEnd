#include "GraphicDeivceDX12.h"
#include "../Common/Source/CGHUtil.h"
#include "../Common/Source/DxException.h"
#include "../Common/Source/DX12SwapChain.h"

#include "GraphicResourceLoader.h"
#include "DX12GarbageFrameResourceMG.h"
#include "Component.h"
#include "Camera.h"

using namespace DirectX;
GraphicDeviceDX12* GraphicDeviceDX12::s_Graphic = nullptr;

enum
{
	ROOT_MAINPASS_CB = 0,

	ROOT_OBJECTINFO_CB,
	ROOT_MATERIAL_CB,
	ROOT_TEXTUREINFO_SRV,
	ROOT_TEXTURE_TABLE,

	ROOT_NORMAL_SRV,
	ROOT_TANGENT_SRV,
	ROOT_BITAN_SRV,
	ROOT_UV,

	ROOT_WEIGHTINFO_SRV,
	ROOT_WEIGHT_SRV,
	ROOT_BONEDATA_SRV,
	ROOT_NUM,
};

GraphicDeviceDX12* GraphicDeviceDX12::GetGraphic()
{
	return s_Graphic;
}

ID3D12Device* GraphicDeviceDX12::GetDevice()
{
	return s_Graphic->m_d3dDevice.Get();
}

D3D12_CPU_DESCRIPTOR_HANDLE GraphicDeviceDX12::GetCurrPresentRTV()
{
	return m_swapChain->CurrRTV();
}

D3D12_CPU_DESCRIPTOR_HANDLE GraphicDeviceDX12::GetPresentDSV()
{
	return m_swapChain->GetDSV();
}

D3D12_GPU_VIRTUAL_ADDRESS GraphicDeviceDX12::GetCurrMainPassCBV()
{
	return m_passCBs[m_currFrame]->Resource()->GetGPUVirtualAddress();
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

void GraphicDeviceDX12::BaseRender()
{
	m_cmdList->SetPipelineState(DX12PipelineMG::instance.GetGraphicPipeline("TestScene"));
	m_cmdList->SetGraphicsRootSignature(DX12PipelineMG::instance.GetRootSignature("TestScene"));

	D3D12_CPU_DESCRIPTOR_HANDLE presentRTV[] = { GetCurrPresentRTV() };
	auto presentDSV = GetPresentDSV();
	m_cmdList->OMSetRenderTargets(1, presentRTV, false, &presentDSV);
	m_cmdList->SetGraphicsRootConstantBufferView(ROOT_MAINPASS_CB, GetCurrMainPassCBV());

	for (auto& iter : m_skinnedMeshRenderQueue.queue)
	{
		COMMaterial* matCom = iter.first->GetComponent<COMMaterial>();
		COMSkinnedMesh* meshCom = iter.first->GetComponent<COMSkinnedMesh>();

		m_cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		D3D12_GPU_VIRTUAL_ADDRESS matCB = matCom->GetMaterialDataGPU();
		const CGHMesh* currMesh = meshCom->GetMeshData();
		auto textureDescHeap = matCom->GetTextureHeap();
		auto descHeapHandle = textureDescHeap->GetGPUDescriptorHandleForHeapStart();

		ID3D12DescriptorHeap* heaps[] = { textureDescHeap };
		m_cmdList->SetDescriptorHeaps(_countof(heaps), heaps);

		m_cmdList->SetGraphicsRootConstantBufferView(ROOT_MATERIAL_CB, matCB);
		m_cmdList->SetGraphicsRootDescriptorTable(ROOT_TEXTURE_TABLE, descHeapHandle);

		D3D12_VERTEX_BUFFER_VIEW vbView = {};
		vbView.BufferLocation = currMesh->meshData[MESHDATA_POSITION]->GetGPUVirtualAddress();
		vbView.SizeInBytes = sizeof(aiVector3D) * currMesh->numData[MESHDATA_POSITION];
		vbView.StrideInBytes = sizeof(aiVector3D);

		D3D12_INDEX_BUFFER_VIEW ibView = {};
		ibView.BufferLocation = currMesh->meshData[MESHDATA_INDEX]->GetGPUVirtualAddress();
		ibView.Format = DXGI_FORMAT_R32_UINT;
		ibView.SizeInBytes = sizeof(unsigned int) * currMesh->numData[MESHDATA_INDEX];

		m_cmdList->IASetVertexBuffers(0, 1, &vbView);
		m_cmdList->IASetIndexBuffer(&ibView);

		m_cmdList->SetGraphicsRootShaderResourceView(ROOT_NORMAL_SRV, currMesh->meshData[MESHDATA_NORMAL]->GetGPUVirtualAddress());
		m_cmdList->SetGraphicsRootShaderResourceView(ROOT_TANGENT_SRV, currMesh->meshData[MESHDATA_TAN]->GetGPUVirtualAddress());
		m_cmdList->SetGraphicsRootShaderResourceView(ROOT_BITAN_SRV, currMesh->meshData[MESHDATA_BITAN]->GetGPUVirtualAddress());
		m_cmdList->SetGraphicsRootShaderResourceView(ROOT_UV, currMesh->meshDataUVs[0]->GetGPUVirtualAddress());

		m_cmdList->SetGraphicsRootShaderResourceView(ROOT_WEIGHTINFO_SRV, currMesh->boneWeightInfos->GetGPUVirtualAddress());
		m_cmdList->SetGraphicsRootShaderResourceView(ROOT_WEIGHT_SRV, currMesh->boneWeights->GetGPUVirtualAddress());

		m_cmdList->SetGraphicsRootShaderResourceView(ROOT_BONEDATA_SRV, meshCom->GetBoneData(m_currFrame));

		m_cmdList->DrawIndexedInstanced(currMesh->numData[MESHDATA_INDEX], 1, 0, 0, 0);
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

		m_cmdListAllocs.resize(m_numFrameResource * 2);
		for (size_t i = 0; i < m_numFrameResource * 2; i++)
		{
			ThrowIfFailed(m_d3dDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
				IID_PPV_ARGS(m_cmdListAllocs[i].GetAddressOf())));
		}

		ThrowIfFailed(m_d3dDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
			m_cmdListAllocs.front().Get(), nullptr, IID_PPV_ARGS(m_cmdList.GetAddressOf())));

		ThrowIfFailed(m_cmdList->Close());

		m_cmdListAllocs.front().Reset();

		ThrowIfFailed(m_d3dDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
			m_cmdListAllocs.front().Get(), nullptr, IID_PPV_ARGS(m_dataLoaderCmdList.GetAddressOf())));

		ThrowIfFailed(m_dataLoaderCmdList->Close());

		ThrowIfFailed(m_d3dDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE,
			IID_PPV_ARGS(m_fence.GetAddressOf())));
	}

	m_swapChain->CreateSwapChain(hWnd, m_commandQueue.Get(), m_backBufferFormat, windowWidth, windowHeight, 2);

	OnResize(windowWidth, windowHeight);
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

	auto cmdListAlloc = GetCurrRenderBeginCommandAllocator();

	ThrowIfFailed(m_cmdList->Reset(cmdListAlloc, nullptr));
	{
		m_swapChain->ReSize(m_cmdList.Get(), windowWidth, windowHeight);
	}
	ThrowIfFailed(m_cmdList->Close());

	ID3D12CommandList* cmdLists[] = { m_cmdList.Get() };
	m_commandQueue->ExecuteCommandLists(_countof(cmdLists), cmdLists);

	float fovAngleY = 0.785398163f;
	float aspectRatio = (float)windowWidth / windowHeight;
	float fovAngleX = 2 * atanf(aspectRatio * tanf(fovAngleY));

	XMMATRIX proj = XMMatrixPerspectiveFovLH(fovAngleY, aspectRatio, 1.0f, 1000.0f);
	XMStoreFloat4x4(&m_projMat, proj);

	FlushCommandQueue();
	ThrowIfFailed(cmdListAlloc->Reset());
}

void GraphicDeviceDX12::LoadMeshDataFile(const char* filePath, std::vector<CGHMesh>* outMeshSet,
	std::vector<CGHMaterial>* outMaterials = nullptr, std::vector<CGHNode>* outNode)
{
	DX12GraphicResourceLoader loader;
	std::vector<ComPtr<ID3D12Resource>> upBuffers;

	auto allocator = DX12GarbageFrameResourceMG::s_instance.RentCommandAllocator();
	ThrowIfFailed(m_dataLoaderCmdList->Reset(allocator.Get(), nullptr));
	loader.LoadAllData(filePath, aiComponent_CAMERAS | aiComponent_TEXTURES | aiComponent_COLORS | aiComponent_LIGHTS,
		m_dataLoaderCmdList.Get(), &upBuffers, outMeshSet, outMaterials, outNode);

	ThrowIfFailed(m_dataLoaderCmdList->Close());

	ID3D12CommandList* cmdLists[] = { m_dataLoaderCmdList.Get() };
	m_commandQueue->ExecuteCommandLists(_countof(cmdLists), cmdLists);

	DX12GarbageFrameResourceMG::s_instance.RegistGarbeges(m_commandQueue.Get(), upBuffers, allocator);
}

void GraphicDeviceDX12::RenderBegin()
{
	auto cmdListAlloc = GetCurrRenderBeginCommandAllocator();

	ThrowIfFailed(cmdListAlloc->Reset());
	ThrowIfFailed(m_cmdList->Reset(cmdListAlloc, nullptr));

	m_swapChain->RenderBegin(m_cmdList.Get(), DirectX::Colors::Gray);

	m_cmdList->RSSetViewports(1, &m_screenViewport);
	m_cmdList->RSSetScissorRects(1, &m_scissorRect);
}

void GraphicDeviceDX12::RenderMesh(CGHNode* node, unsigned int renderFlag)
{
	m_meshRenderQueue.queue.push_back({ node, renderFlag });
}

void GraphicDeviceDX12::RenderSkinnedMesh(CGHNode* node, unsigned int renderFlag)
{
	m_skinnedMeshRenderQueue.queue.push_back({ node, renderFlag });
}

void GraphicDeviceDX12::RenderUI(CGHNode* node, unsigned int renderFlag)
{
	m_uiRenderQueue.queue.push_back({ node, renderFlag });
}

void GraphicDeviceDX12::RenderEnd()
{
	BaseRender();

	m_meshRenderQueue.queue.clear();
	m_skinnedMeshRenderQueue.queue.clear();
	m_uiRenderQueue.queue.clear();

	m_swapChain->RenderEnd(m_cmdList.Get());

	ThrowIfFailed(m_cmdList->Close());
	ID3D12CommandList* cmdsLists[] = { m_cmdList.Get() };
	m_commandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	m_swapChain->Present();
	m_currentFence++;
	m_fenceCounts[m_currFrame] = m_currentFence;
	ThrowIfFailed(m_commandQueue->Signal(m_fence.Get(), m_currentFence));

	m_currFrame = (m_currFrame + 1) % m_numFrameResource;

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

ID3D12CommandAllocator* GraphicDeviceDX12::GetCurrRenderBeginCommandAllocator()
{
	return m_cmdListAllocs[m_currFrame].Get();
}

ID3D12CommandAllocator* GraphicDeviceDX12::GetCurrRenderEndCommandAllocator()
{
	return m_cmdListAllocs[m_currFrame + m_numFrameResource].Get();
}

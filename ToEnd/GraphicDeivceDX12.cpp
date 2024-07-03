#include "imgui.h"
#include "backends/imgui_impl_win32.h"
#include "backends/imgui_impl_dx12.h"

#include "GraphicDeivceDX12.h"

#include "../Common/Source/CGHUtil.h"
#include "../Common/Source/DxException.h"
#include "../Common/Source/DX12SwapChain.h"

#include "GraphicResourceLoader.h"
#include "DX12GarbageFrameResourceMG.h"
#include "DX12SMAA.h"
#include "DX12PipelineMG.h"
#include "BaseComponents.h"
#include "LightComponents.h"
#include "Camera.h"
#include "Mouse.h"
#include "InputManager.h"



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

const PipeLineWorkSet* GraphicDeviceDX12::GetPSOWorkset(const char* psowName)
{
	PipeLineWorkSet* result = nullptr;

	auto iter = m_meshPSOWorkSets.find(psowName);

	if (iter != m_meshPSOWorkSets.end())
	{
		result = &iter->second;
	}

	return result;
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

GraphicDeviceDX12::GraphicDeviceDX12()
{
	m_smaa = new DX12SMAA;
}

GraphicDeviceDX12::~GraphicDeviceDX12()
{
	if (m_smaa != nullptr)
	{
		delete m_smaa;
	}

	ImGui_ImplDX12_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
}

void GraphicDeviceDX12::BaseRender()
{
	for (auto& currPSOW : m_basePSOWorkSets)
	{
		if (currPSOW.pso)
		{
			m_cmdList->SetPipelineState(currPSOW.pso);

			switch (currPSOW.psoType)
			{
			case 0:
			{
				m_cmdList->SetGraphicsRootSignature(currPSOW.rootSig);
			}
			break;
			case 1:
			{
				m_cmdList->SetComputeRootSignature(currPSOW.rootSig);
			}
			break;
			default:
				assert(false);
				break;
			}

			if (currPSOW.baseGraphicCmdFunc != nullptr)
			{
				currPSOW.baseGraphicCmdFunc(m_cmdList.Get());
			}
		}
	}

	for (auto& currQueue : m_renderQueues)
	{
		currQueue.queue.clear();
	}
}

void GraphicDeviceDX12::ReservedUIInfoSort()
{

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
	m_rtvSize = m_d3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	m_srvcbvuavSize = m_d3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	m_dsvSize = m_d3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

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

		m_cmdListAllocs.front()->Reset();

		ThrowIfFailed(m_d3dDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
			m_cmdListAllocs.front().Get(), nullptr, IID_PPV_ARGS(m_dataLoaderCmdList.GetAddressOf())));

		ThrowIfFailed(m_dataLoaderCmdList->Close());

		ThrowIfFailed(m_d3dDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE,
			IID_PPV_ARGS(m_fence.GetAddressOf())));
	}

	m_swapChain->CreateSwapChain(hWnd, m_commandQueue.Get(), m_backBufferFormat, windowWidth, windowHeight, 2);

	OnResize(windowWidth, windowHeight);

	ThrowIfFailed(m_cmdList->Reset(m_cmdListAllocs.front().Get(), nullptr));
	{
		D3D12_HEAP_PROPERTIES prop = {};
		D3D12_RESOURCE_DESC reDesc = {};
		D3D12_HEAP_FLAGS flags = D3D12_HEAP_FLAG_NONE;

		prop.Type = D3D12_HEAP_TYPE_READBACK;

		reDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		reDesc.DepthOrArraySize = 1;
		reDesc.Height = 1;
		reDesc.Width = sizeof(UINT16) * m_numFrameResource;
		reDesc.Format = DXGI_FORMAT_UNKNOWN;
		reDesc.MipLevels = 1;
		reDesc.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
		reDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		reDesc.SampleDesc.Count = 1;

		ThrowIfFailed(m_d3dDevice->CreateCommittedResource(&prop, flags, &reDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(m_renderIDatMouseRead.GetAddressOf())));
	}

	m_dirLightDatas = std::make_unique<DX12UploadBuffer<DX12DirLightData>>(m_d3dDevice.Get(), m_numMaxDirLight * m_numFrameResource, false);
	m_shadowPassCB = std::make_unique<DX12UploadBuffer<DirectX::XMMATRIX>>(m_d3dDevice.Get(), m_numMaxShadowMap * m_numFrameResource, true);

	ThrowIfFailed(m_cmdList->Close());

	ID3D12CommandList* cmdLists[] = { m_cmdList.Get() };
	m_commandQueue->ExecuteCommandLists(_countof(cmdLists), cmdLists);

	FlushCommandQueue();

	ThrowIfFailed(m_cmdListAllocs.front()->Reset());

	BuildPso();

	{
		D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};

		heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		heapDesc.NumDescriptors = 2;
		heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

		ThrowIfFailed(m_d3dDevice->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(m_uiRenderSRVHeap.GetAddressOf())));
	}

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	ImGui::StyleColorsDark();

	ImGui_ImplWin32_Init(hWnd);
	ImGui_ImplDX12_Init(m_d3dDevice.Get(), m_numFrameResource, m_backBufferFormat, m_uiRenderSRVHeap.Get(), m_uiRenderSRVHeap->GetCPUDescriptorHandleForHeapStart(),
		m_uiRenderSRVHeap->GetGPUDescriptorHandleForHeapStart());
}

void GraphicDeviceDX12::Update(float delta, const Camera* camera)
{
	assert(camera);

	DX12PassConstants passCons;

	XMVECTOR deter;
	XMMATRIX xmView = XMLoadFloat4x4(camera->GetViewMatrix());
	XMMATRIX xmProj = XMLoadFloat4x4(&m_projMat);
	XMMATRIX xmViewProj = XMMatrixMultiply(xmView, xmProj);

	XMMATRIX xminvView = XMMatrixInverse(&XMMatrixDeterminant(xmView), xmView);
	XMMATRIX xminvProj = XMMatrixInverse(&XMMatrixDeterminant(xmProj), xmProj);
	XMMATRIX xminvViewProj = XMMatrixInverse(&XMMatrixDeterminant(xmViewProj), xmViewProj);
	XMStoreFloat4x4(&m_cameraViewProjMat, xmViewProj);

	XMStoreFloat4x4(&passCons.view, XMMatrixTranspose(xmView));
	XMStoreFloat4x4(&passCons.proj, XMMatrixTranspose(xmProj));
	XMStoreFloat4x4(&passCons.viewProj, XMMatrixTranspose(xmViewProj));
	XMStoreFloat4x4(&passCons.invView, XMMatrixTranspose(xminvView));
	XMStoreFloat4x4(&passCons.invProj, XMMatrixTranspose(xminvProj));
	XMStoreFloat4x4(&passCons.invViewProj, XMMatrixTranspose(xminvViewProj));

	passCons.renderTargetSizeX = m_scissorRect.right;
	passCons.renderTargetSizeY = m_scissorRect.bottom;
	passCons.invRenderTargetSize = { 1.0f / m_screenViewport.Width, 1.0f / m_screenViewport.Height };
	passCons.samplerIndex = 2;
	passCons.eyePosW = camera->GetEyePos();
	passCons.ambientLight = { 0.25f, 0.25f, 0.35f, 1.0f }; //TODO ambientLight
	auto mouseState = InputManager::GetMouse().GetLastState();
	passCons.mousePos.x = mouseState.x;
	passCons.mousePos.y = mouseState.y;

	m_passCBs[m_currFrame]->CopyData(0, &passCons);

	XMVECTOR rayOrigin = XMVectorZero();
	XMVECTOR ray = XMLoadFloat3(&camera->GetViewRay(m_projMat, static_cast<unsigned int>(m_screenViewport.Width), static_cast<unsigned int>(m_screenViewport.Height)));

	rayOrigin = XMVector3Transform(rayOrigin, xminvView);
	ray = (xminvView.r[0] * ray.m128_f32[0]) + (xminvView.r[1] * ray.m128_f32[1]) + (xminvView.r[2] * ray.m128_f32[2]);

	m_cameraDistance = camera->GetDistance();
	m_cameraTargetPos = camera->GetTargetPos();
	XMStoreFloat3(&m_ray, ray);
	XMStoreFloat3(&m_rayOrigin, rayOrigin);
}

void GraphicDeviceDX12::RenderMesh(const PipeLineWorkSet* psow, CGHNode* node)
{
	if (psow->pso)
	{
		if (GlobalOptions::GO.GRAPHIC.EnableWireFrame && psow->wireframePSO)
		{
			m_cmdList->SetPipelineState(psow->wireframePSO);
		}
		else
		{
			m_cmdList->SetPipelineState(psow->pso);
		}

		switch (psow->psoType)
		{
		case 0:
		{
			m_cmdList->SetGraphicsRootSignature(psow->rootSig);
		}
		break;
		case 1:
		{
			m_cmdList->SetComputeRootSignature(psow->rootSig);
		}
		break;
		default:
			assert(false);
			break;
		}

		if (psow->baseGraphicCmdFunc != nullptr)
		{
			psow->baseGraphicCmdFunc(m_cmdList.Get());
		}

		if (psow->nodeGraphicCmdFunc != nullptr)
		{
			psow->nodeGraphicCmdFunc(m_cmdList.Get(), node);
		}
	}
}

void GraphicDeviceDX12::RenderMeshs(const PipeLineWorkSet* psow, const std::vector<CGHNode*>& nodes)
{
	if (psow->pso)
	{
		m_cmdList->SetPipelineState(psow->pso);

		switch (psow->psoType)
		{
		case 0:
		{
			m_cmdList->SetGraphicsRootSignature(psow->rootSig);
		}
		break;
		case 1:
		{
			m_cmdList->SetComputeRootSignature(psow->rootSig);
		}
		break;
		default:
			assert(false);
			break;
		}

		if (psow->baseGraphicCmdFunc != nullptr)
		{
			psow->baseGraphicCmdFunc(m_cmdList.Get());
		}

		if (psow->nodeGraphicCmdFunc != nullptr)
		{
			for (auto& currNode : nodes)
			{
				psow->nodeGraphicCmdFunc(m_cmdList.Get(), currNode);
			}
		}
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

	auto cmdListAlloc = GetCurrRenderBeginCommandAllocator();

	ThrowIfFailed(m_cmdList->Reset(cmdListAlloc, nullptr));
	{
		m_swapChain->ReSize(m_cmdList.Get(), windowWidth, windowHeight);
		CreateDeferredTextures(windowWidth, windowHeight);
	}

	std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> smaaUPloadBuffers;
	m_smaa->Resize(m_d3dDevice.Get(), m_cmdList.Get(),
		m_deferredResources[DEFERRED_TEXTURE_HDR_DIFFUSE].Get(), m_deferredFormat[DEFERRED_TEXTURE_HDR_DIFFUSE],
		windowWidth, windowHeight, smaaUPloadBuffers);

	m_smaaResult = nullptr;
	{
		if (m_diffuseTextureRTVHeap == nullptr)
		{
			D3D12_DESCRIPTOR_HEAP_DESC heapdesc = {};
			heapdesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE::D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
			heapdesc.NodeMask = 0;
			heapdesc.NumDescriptors = 1;
			heapdesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
			ThrowIfFailed(m_d3dDevice->CreateDescriptorHeap(&heapdesc, IID_PPV_ARGS(m_diffuseTextureRTVHeap.GetAddressOf())));
		}

		D3D12_RESOURCE_DESC texDesc = m_deferredResources[DEFERRED_TEXTURE_HDR_DIFFUSE]->GetDesc();
		D3D12_HEAP_PROPERTIES heapProp = {};
		D3D12_HEAP_FLAGS heapFlags = D3D12_HEAP_FLAG_NONE;
		m_deferredResources[DEFERRED_TEXTURE_HDR_DIFFUSE]->GetHeapProperties(&heapProp, &heapFlags);

		D3D12_CLEAR_VALUE clearValue = {};
		std::memcpy(clearValue.Color, &GlobalOptions::GO.GRAPHIC.BGColor, sizeof(DirectX::XMVECTORF32));
		clearValue.Format = texDesc.Format;

		ThrowIfFailed(m_d3dDevice->CreateCommittedResource(&heapProp, heapFlags, &texDesc,
			D3D12_RESOURCE_STATE_RENDER_TARGET, &clearValue, IID_PPV_ARGS(m_smaaResult.GetAddressOf())));

		D3D12_RENDER_TARGET_VIEW_DESC viewDesc = {};
		viewDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
		viewDesc.Texture2D.MipSlice = 0;
		viewDesc.Texture2D.PlaneSlice = 0;
		viewDesc.Format = texDesc.Format;

		auto descHeap = m_diffuseTextureRTVHeap->GetCPUDescriptorHandleForHeapStart();
		m_d3dDevice->CreateRenderTargetView(m_smaaResult.Get(), &viewDesc, descHeap);

		ReservationResourceBarrierBeforeRenderStart(CD3DX12_RESOURCE_BARRIER::Transition(m_smaaResult.Get(),
			D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ));
	}

	ThrowIfFailed(m_cmdList->Close());
	ID3D12CommandList* cmdLists[] = { m_cmdList.Get() };
	m_commandQueue->ExecuteCommandLists(_countof(cmdLists), cmdLists);

	float fovAngleY = 0.785398163f;
	float aspectRatio = (float)windowWidth / windowHeight;
	float fovAngleX = 2 * atanf(aspectRatio * tanf(fovAngleY));

	XMMATRIX proj = XMMatrixPerspectiveFovLH(fovAngleY, aspectRatio, 0.1f, 1000.0f);
	XMStoreFloat4x4(&m_projMat, proj);

	FlushCommandQueue();

	ThrowIfFailed(cmdListAlloc->Reset());
}

void GraphicDeviceDX12::LoadMeshDataFile(const char* filePath, bool triangleCw, std::vector<CGHMesh>* outMeshSet,
	std::vector<CGHMaterial>* outMaterials, std::vector<CGHNode>* outNode)
{
	DX12GraphicResourceLoader loader;
	std::vector<ComPtr<ID3D12Resource>> upBuffers;

	auto allocator = DX12GarbageFrameResourceMG::s_instance.RentCommandAllocator();
	ThrowIfFailed(m_dataLoaderCmdList->Reset(allocator.Get(), nullptr));
	loader.LoadAllData(filePath, aiComponent_CAMERAS | aiComponent_TEXTURES | aiComponent_COLORS | aiComponent_LIGHTS, triangleCw,
		m_dataLoaderCmdList.Get(), &upBuffers, outMeshSet, outMaterials, outNode);

	ThrowIfFailed(m_dataLoaderCmdList->Close());

	ID3D12CommandList* cmdLists[] = { m_dataLoaderCmdList.Get() };
	m_commandQueue->ExecuteCommandLists(_countof(cmdLists), cmdLists);

	DX12GarbageFrameResourceMG::s_instance.RegistGarbeges(m_commandQueue.Get(), upBuffers, allocator);
}

void GraphicDeviceDX12::ReservationResourceBarrierBeforeRenderStart(const CD3DX12_RESOURCE_BARRIER& barrier)
{
	m_beforeRenderStartResourceBarriers.emplace_back(barrier);
}

void GraphicDeviceDX12::RenderBegin()
{
	auto cmdListAlloc = GetCurrRenderBeginCommandAllocator();

	ThrowIfFailed(cmdListAlloc->Reset());
	ThrowIfFailed(m_cmdList->Reset(cmdListAlloc, nullptr));

	if (m_beforeRenderStartResourceBarriers.size())
	{
		m_cmdList->ResourceBarrier(m_beforeRenderStartResourceBarriers.size(), m_beforeRenderStartResourceBarriers.data());
		m_beforeRenderStartResourceBarriers.clear();
	}

	m_swapChain->RenderBegin(m_cmdList.Get(), GlobalOptions::GO.GRAPHIC.BGColor);

	m_cmdList->RSSetViewports(1, &m_screenViewport);
	m_cmdList->RSSetScissorRects(1, &m_scissorRect);

	CD3DX12_RESOURCE_BARRIER resourceBars[DEFERRED_TEXTURE_NUM] = {};

	for (int i = 0; i < DEFERRED_TEXTURE_NUM - 1; i++)
	{
		resourceBars[i] = CD3DX12_RESOURCE_BARRIER::Transition(m_deferredResources[i + 1].Get(),
			D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET);
	}

	m_cmdList->ResourceBarrier(_countof(resourceBars), resourceBars);

	D3D12_CPU_DESCRIPTOR_HANDLE renderIDTexture = m_deferredRTVHeap->GetCPUDescriptorHandleForHeapStart();
	D3D12_CPU_DESCRIPTOR_HANDLE renderDiffuseTexture = m_diffuseTextureRTVHeap->GetCPUDescriptorHandleForHeapStart();
	D3D12_CPU_DESCRIPTOR_HANDLE smaaResultDiffuseTexture = GetSMAAResultRTV();
	renderIDTexture.ptr += m_rtvSize * DEFERRED_TEXTURE_RENDERID;

	m_cmdList->ClearRenderTargetView(renderIDTexture, GlobalOptions::GO.GRAPHIC.BGColor, 0, nullptr);
	m_cmdList->ClearRenderTargetView(renderDiffuseTexture, GlobalOptions::GO.GRAPHIC.BGColor, 0, nullptr);
	m_cmdList->ClearRenderTargetView(smaaResultDiffuseTexture, GlobalOptions::GO.GRAPHIC.BGColor, 0, nullptr);

	ImGui_ImplDX12_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
}

void GraphicDeviceDX12::RenderLight(CGHNode* node, unsigned int lightFlags, size_t lightType)
{
	if (lightType == typeid(COMDirLight).hash_code())
	{
		m_renderQueues[RENDERQUEUE_LIGHT_DIR].queue.push_back({ node, lightFlags });
	}
	else if (lightType == typeid(COMPointLight).hash_code())
	{
		m_renderQueues[RENDERQUEUE_LIGHT_POINT].queue.push_back({ node, lightFlags });
	}
}

void GraphicDeviceDX12::RenderEnd()
{
	CreateRenderResources();
	BaseRender();

	{
		auto renderIDTexture = m_deferredResources[DEFERRED_TEXTURE_RENDERID].Get();

		D3D12_RESOURCE_BARRIER bar = {};
		bar.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		bar.Transition.pResource = renderIDTexture;
		bar.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
		bar.Transition.StateAfter = D3D12_RESOURCE_STATE_GENERIC_READ;
		bar.Transition.Subresource = 0;

		D3D12_TEXTURE_COPY_LOCATION src = {};
		src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
		src.pResource = renderIDTexture;
		src.SubresourceIndex = 0;

		D3D12_TEXTURE_COPY_LOCATION dest = {};
		dest.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
		dest.pResource = m_renderIDatMouseRead.Get();
		dest.PlacedFootprint.Offset = sizeof(UINT16) * m_currFrame;
		dest.PlacedFootprint.Footprint.Format = DXGI_FORMAT_R16_UINT;
		dest.PlacedFootprint.Footprint.RowPitch = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
		dest.PlacedFootprint.Footprint.Height = 1;
		dest.PlacedFootprint.Footprint.Width = sizeof(UINT16);
		dest.PlacedFootprint.Footprint.Depth = 1;
		dest.SubresourceIndex = 0;

		auto mouseState = InputManager::GetMouse().GetLastState();

		m_cmdList->ResourceBarrier(1, &bar);

		if (mouseState.x < GlobalOptions::GO.WIN.WindowsizeX && mouseState.y < GlobalOptions::GO.WIN.WindowsizeY
			&& mouseState.x > 0 && mouseState.y > 0)
		{
			D3D12_BOX rect = {};

			rect.left = mouseState.x;
			rect.right = mouseState.x + 1;
			rect.top = mouseState.y;
			rect.bottom = mouseState.y + 1;
			rect.front = 0;
			rect.back = 1;

			m_cmdList->CopyTextureRegion(&dest, 0, 0, 0, &src, &rect);
		}

		bar.Transition.StateBefore = D3D12_RESOURCE_STATE_GENERIC_READ;
		bar.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;

		m_cmdList->ResourceBarrier(1, &bar);
	}

	{
		/*	static bool test = false;
			ImGui::ShowDemoWindow(&test);*/
		GraphicOptionGUIRender();
		ImGui::Render();

		m_cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_swapChain->GetCurrRenderTargetResource(),
			D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET));
		m_cmdList->OMSetRenderTargets(1, &m_swapChain->CurrRTV(), false, nullptr);
		m_cmdList->SetDescriptorHeaps(1, m_uiRenderSRVHeap.GetAddressOf());
		ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), m_cmdList.Get());
	}

	m_swapChain->RenderEnd(m_cmdList.Get());

	if (m_afterRenderEndResourceBarriers.size())
	{
		m_cmdList->ResourceBarrier(m_afterRenderEndResourceBarriers.size(), m_afterRenderEndResourceBarriers.data());
		m_afterRenderEndResourceBarriers.clear();
	}

	ThrowIfFailed(m_cmdList->Close());
	ID3D12CommandList* cmdsLists[] = { m_cmdList.Get() };
	m_commandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	m_swapChain->Present();
	m_currentFence++;
	m_fenceCounts[m_currFrame] = m_currentFence;
	ThrowIfFailed(m_commandQueue->Signal(m_fence.Get(), m_currentFence));

	unsigned int currExcuteFrame = m_currFrame;
	m_currFrame = (m_currFrame + 1) % m_numFrameResource;

	auto completedValue = m_fence->GetCompletedValue();
	if (completedValue < m_fenceCounts[m_currFrame])
	{
		HANDLE eventHandle = CreateEventEx(nullptr, nullptr, false, EVENT_ALL_ACCESS);
		ThrowIfFailed(m_fence->SetEventOnCompletion(m_fenceCounts[m_currFrame], eventHandle));

		if (eventHandle)
		{
			WaitForSingleObject(eventHandle, INFINITE);
			CloseHandle(eventHandle);
			completedValue = m_fenceCounts[m_currFrame];
		}
		else
		{
			assert(false);
		}
	}

	unsigned int targetFrame = currExcuteFrame;
	for (unsigned int i = 0; i < m_numFrameResource; i++)
	{
		if (completedValue >= m_fenceCounts[targetFrame])
		{
			BYTE* mapped = nullptr;
			D3D12_RANGE range = {};
			range.Begin = sizeof(UINT16) * targetFrame;
			range.End = range.Begin + sizeof(UINT16);

			m_renderIDatMouseRead->Map(0, &range, reinterpret_cast<void**>(&mapped));
			std::memcpy(&m_currMouseTargetRenderID, mapped, sizeof(UINT16));
			m_renderIDatMouseRead->Unmap(0, nullptr);
			break;
		}

		targetFrame = (targetFrame + m_numFrameResource - 1) % m_numFrameResource;
	}
}

void GraphicDeviceDX12::CreateRenderResources()
{
	for (auto& currNode : m_renderQueues[RENDERQUEUE_LIGHT_DIR].queue)
	{
		if (currNode.second & CGHLightComponent::LIGHT_FLAG_SHADOW)
		{
			auto comLight = currNode.first->GetComponent<COMDirLight>();

			if (comLight != nullptr)
			{
				auto iter = m_dirLightShadowMaps.find(comLight);

				if (iter == m_dirLightShadowMaps.end())
				{
					auto& currShadowMap = m_dirLightShadowMaps[comLight];

					D3D12_RESOURCE_DESC desc = m_swapChain->GetDSResource()->GetDesc();
					D3D12_HEAP_PROPERTIES prop;
					D3D12_HEAP_FLAGS flags;
					m_swapChain->GetDSResource()->GetHeapProperties(&prop, &flags);

					D3D12_CLEAR_VALUE clearValue = {};
					clearValue.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
					clearValue.DepthStencil.Depth = 1.0f;
					clearValue.DepthStencil.Stencil = 0;
					ThrowIfFailed(m_d3dDevice->CreateCommittedResource(&prop, D3D12_HEAP_FLAG_NONE, &desc,
						D3D12_RESOURCE_STATE_DEPTH_WRITE, &clearValue, IID_PPV_ARGS(currShadowMap.resource.GetAddressOf())));

					currShadowMap.resource->SetName(L"shadowMapResource");

					D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
					heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
					heapDesc.NumDescriptors = 1;
					ThrowIfFailed(m_d3dDevice->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(currShadowMap.texDSVHeap.GetAddressOf())));

					D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
					dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
					dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
					dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
					dsvDesc.Texture2D.MipSlice = 0;
					m_d3dDevice->CreateDepthStencilView(currShadowMap.resource.Get(), &dsvDesc,
						currShadowMap.texDSVHeap->GetCPUDescriptorHandleForHeapStart());

					for (auto& iter : m_lightRenderSRVHeaps)
					{
						auto texSRVHeapCPU = iter.second->GetCPUDescriptorHandleForHeapStart();
						texSRVHeapCPU.ptr += (DEFERRED_TEXTURE_NUM + m_currNumShadowMap) * m_srvcbvuavSize;
						D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
						srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
						srvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
						srvDesc.Texture2D.MipLevels = 1;
						srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
						m_d3dDevice->CreateShaderResourceView(currShadowMap.resource.Get(), &srvDesc, texSRVHeapCPU);
					}

					currShadowMap.srvHeapIndex = m_currNumShadowMap++;

					comLight->AddDeleteEvent([this](Component* comp)
						{
							m_dirLightShadowMaps.erase(comp);
						});
				}
			}
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

void GraphicDeviceDX12::BuildPso()
{
	BuildSkinnedMeshBoneUpdateComputePipeLineWorkSet();
	BuildDeferredSkinnedMeshPipeLineWorkSet();
	BuildShadowMapWritePipeLineWorkSet();
	BuildDeferredLightDirPipeLineWorkSet();
	BuildSMAARenderPipeLineWorkSet();
	BuildPostProcessingPipeLineWorkSet();
	BuildTextureDataDebugPipeLineWorkSet();
}

void GraphicDeviceDX12::CreateDeferredTextures(int windowWidth, int windowHeight)
{
	D3D12_HEAP_PROPERTIES prop = {};
	D3D12_RESOURCE_DESC rDesc = {};

	prop.Type = D3D12_HEAP_TYPE_DEFAULT;
	prop.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	prop.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	prop.CreationNodeMask = 0;
	prop.VisibleNodeMask = 0;

	rDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	rDesc.Alignment = 0;
	rDesc.Width = windowWidth;
	rDesc.Height = windowHeight;
	rDesc.DepthOrArraySize = 1;
	rDesc.MipLevels = 1;
	rDesc.SampleDesc.Count = 1;
	rDesc.SampleDesc.Quality = 0;
	rDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	rDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

	for (unsigned int i = 0; i < DEFERRED_TEXTURE_NUM; i++)
	{
		D3D12_CLEAR_VALUE clearValue = {};
		clearValue.Format = m_deferredFormat[i];
		std::memcpy(clearValue.Color, &GlobalOptions::GO.GRAPHIC.BGColor, sizeof(DirectX::XMVECTORF32));
		m_deferredResources[i] = nullptr;
		rDesc.Format = m_deferredFormat[i];

		ThrowIfFailed(m_d3dDevice->CreateCommittedResource(&prop, D3D12_HEAP_FLAG_NONE, &rDesc,
			D3D12_RESOURCE_STATE_RENDER_TARGET, &clearValue, IID_PPV_ARGS(m_deferredResources[i].GetAddressOf())));

		if (i != DEFERRED_TEXTURE_HDR_DIFFUSE)
		{
			ReservationResourceBarrierBeforeRenderStart(CD3DX12_RESOURCE_BARRIER::Transition(m_deferredResources[i].Get(),
				D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ));
		}
	}

	if (m_deferredRTVHeap == nullptr)
	{
		D3D12_DESCRIPTOR_HEAP_DESC heapdesc = {};
		heapdesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE::D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		heapdesc.NodeMask = 0;
		heapdesc.NumDescriptors = DEFERRED_TEXTURE_NUM;
		heapdesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		ThrowIfFailed(m_d3dDevice->CreateDescriptorHeap(&heapdesc, IID_PPV_ARGS(m_deferredRTVHeap.GetAddressOf())));
	}

	{
		auto heapCPU = m_deferredRTVHeap->GetCPUDescriptorHandleForHeapStart();
		D3D12_RENDER_TARGET_VIEW_DESC viewDesc = {};
		viewDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
		viewDesc.Texture2D.MipSlice = 0;
		viewDesc.Texture2D.PlaneSlice = 0;

		for (int i = 0; i < DEFERRED_TEXTURE_NUM; i++)
		{
			viewDesc.Format = m_deferredFormat[i];
			m_d3dDevice->CreateRenderTargetView(m_deferredResources[i].Get(), &viewDesc, heapCPU);
			heapCPU.ptr += m_rtvSize;
		}
	}

	{
		m_currNumShadowMap = 0;
		m_dirLightShadowMaps.clear();
		m_lightRenderSRVHeaps.clear();

		D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
		heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		heapDesc.NumDescriptors = DEFERRED_TEXTURE_NUM + m_numMaxShadowMap;

		for (int i = 0; i < m_numFrameResource; i++)
		{
			ID3D12Resource* currSwapChainRenderTarget = m_swapChain->GetRenderTragetResource(i);
			auto& currSRVHeap = m_lightRenderSRVHeaps[currSwapChainRenderTarget];
			ThrowIfFailed(m_d3dDevice->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(currSRVHeap.GetAddressOf())));

			auto heapCPU = currSRVHeap->GetCPUDescriptorHandleForHeapStart();
			D3D12_SHADER_RESOURCE_VIEW_DESC viewDesc = {};
			viewDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			viewDesc.ViewDimension = D3D12_SRV_DIMENSION::D3D12_SRV_DIMENSION_TEXTURE2D;
			viewDesc.Texture2D.MipLevels = 1;
			viewDesc.Texture2D.MostDetailedMip = 0;
			viewDesc.Texture2D.ResourceMinLODClamp = 0;
			viewDesc.Texture2D.PlaneSlice = 0;
			viewDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

			viewDesc.Format = m_swapChain->GetPresentBufferFormat();
			m_d3dDevice->CreateShaderResourceView(currSwapChainRenderTarget, &viewDesc, heapCPU);
			heapCPU.ptr += m_srvcbvuavSize;

			for (int i = DEFERRED_TEXTURE_NORMAL; i < DEFERRED_TEXTURE_RENDERID; i++)
			{
				viewDesc.Format = m_deferredFormat[i];
				m_d3dDevice->CreateShaderResourceView(m_deferredResources[i].Get(), &viewDesc, heapCPU);
				heapCPU.ptr += m_srvcbvuavSize;
			}

			viewDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
			m_d3dDevice->CreateShaderResourceView(m_swapChain->GetDSResource(), &viewDesc, heapCPU);
		}
	}
}

void GraphicDeviceDX12::GraphicOptionGUIRender()
{
	if (m_isShowGraphicOption)
	{
		if (ImGui::Begin("GraphicOption", &m_isShowGraphicOption, ImGuiWindowFlags_AlwaysAutoResize))
		{
			ImGui::Checkbox("EnableWireFrame", &GlobalOptions::GO.GRAPHIC.EnableWireFrame);
			ImGui::Checkbox("EnableTextureDebugPSO", &GlobalOptions::GO.GRAPHIC.EnableTextureDebugPSO);
			ImGui::Spacing();

			ImGui::Separator();
			ImGui::DragFloat("PhongTessAlpha", &GlobalOptions::GO.GRAPHIC.PhongTessAlpha, 0.01f, 0.0f, 1.0f);
			ImGui::DragFloat("PhongTessFactor", &GlobalOptions::GO.GRAPHIC.PhongTessFactor, 0.3f, 1.0f, 8.0f, "%.0f");
			ImGui::Spacing();

			ImGui::Separator();
			ImGui::Checkbox("EnableSMAA", &GlobalOptions::GO.GRAPHIC.EnableSMAA);
			ImGui::DragFloat("SMAAEdgeThreshold", &GlobalOptions::GO.GRAPHIC.SMAAEdgeDetectionThreshold, 0.001f, 0.001f, 0.2f);
			ImGui::DragInt("SMAAMaxSearchStep", &GlobalOptions::GO.GRAPHIC.SMAAMaxSearchSteps, 0.3f, 0, 64);
			ImGui::DragInt("SMAAMaxSearchStepDiag", &GlobalOptions::GO.GRAPHIC.SMAAMaxSearchStepsDiag, 0.3f, 0, 32);
			ImGui::DragInt("SMAAConerRounding", &GlobalOptions::GO.GRAPHIC.SMAACornerRounding, 0.3f, 0, 50);
		}

		ImGui::End();
	}
}

D3D12_CPU_DESCRIPTOR_HANDLE GraphicDeviceDX12::GetSMAAResultRTV()
{
	auto descHeap = m_diffuseTextureRTVHeap->GetCPUDescriptorHandleForHeapStart();
	descHeap.ptr += m_rtvSize;
	return descHeap;
}

ID3D12CommandAllocator* GraphicDeviceDX12::GetCurrRenderBeginCommandAllocator()
{
	return m_cmdListAllocs[m_currFrame].Get();
}

ID3D12CommandAllocator* GraphicDeviceDX12::GetCurrRenderEndCommandAllocator()
{
	return m_cmdListAllocs[m_currFrame + m_numFrameResource].Get();
}

void GraphicDeviceDX12::BuildSkinnedMeshBoneUpdateComputePipeLineWorkSet()
{
	enum
	{
		ROOT_COMPUTEPASS_CONST,
		ROOT_MESHDATA_TABLE,
		ROOT_BONEDATA_SRV,
		ROOT_NUM
	};

	std::string pipeLineName = "SkinnedMeshBoneUpdateCompute";
	auto& currPSOWorkSet = m_meshPSOWorkSets[pipeLineName];
	D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc = {};

	{
		std::vector<D3D12_ROOT_PARAMETER> rootParams;
		rootParams.resize(ROOT_NUM);

		rootParams[ROOT_COMPUTEPASS_CONST].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
		rootParams[ROOT_COMPUTEPASS_CONST].Constants.RegisterSpace = 0;
		rootParams[ROOT_COMPUTEPASS_CONST].Constants.ShaderRegister = 0;
		rootParams[ROOT_COMPUTEPASS_CONST].Constants.Num32BitValues = 1;
		rootParams[ROOT_COMPUTEPASS_CONST].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

		D3D12_DESCRIPTOR_RANGE textureTableRange[2] = {};
		textureTableRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		textureTableRange[0].RegisterSpace = 0;
		textureTableRange[0].BaseShaderRegister = 0;
		textureTableRange[0].NumDescriptors = 6;
		textureTableRange[0].OffsetInDescriptorsFromTableStart = 0;

		textureTableRange[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
		textureTableRange[1].RegisterSpace = 0;
		textureTableRange[1].BaseShaderRegister = 0;
		textureTableRange[1].NumDescriptors = 4;
		textureTableRange[1].OffsetInDescriptorsFromTableStart = 6;

		rootParams[ROOT_MESHDATA_TABLE].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		rootParams[ROOT_MESHDATA_TABLE].DescriptorTable.NumDescriptorRanges = 2;
		rootParams[ROOT_MESHDATA_TABLE].DescriptorTable.pDescriptorRanges = textureTableRange;
		rootParams[ROOT_MESHDATA_TABLE].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

		rootParams[ROOT_BONEDATA_SRV].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
		rootParams[ROOT_BONEDATA_SRV].Descriptor.RegisterSpace = 0;
		rootParams[ROOT_BONEDATA_SRV].Descriptor.ShaderRegister = 6;

		D3D12_ROOT_SIGNATURE_DESC rootsigDesc = {};
		rootsigDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
		rootsigDesc.pStaticSamplers = s_StaticSamplers;
		rootsigDesc.NumStaticSamplers = _countof(s_StaticSamplers);
		rootsigDesc.NumParameters = rootParams.size();
		rootsigDesc.pParameters = rootParams.data();

		psoDesc.pRootSignature = DX12PipelineMG::instance.CreateRootSignature(pipeLineName.c_str(), &rootsigDesc);
		currPSOWorkSet.rootSig = psoDesc.pRootSignature;
	}

	psoDesc.CS = DX12PipelineMG::instance.CreateShader(DX12_SHADER_COMPUTE, pipeLineName.c_str(), L"Shader/ComputBoneDataToVertex.hlsl", "CS");

	currPSOWorkSet.psoType = 1;
	currPSOWorkSet.pso = DX12PipelineMG::instance.CreateComputePipeline(pipeLineName.c_str(), &psoDesc);
	currPSOWorkSet.baseGraphicCmdFunc = nullptr;
	currPSOWorkSet.nodeGraphicCmdFunc = [this](ID3D12GraphicsCommandList* cmd, CGHNode* node)
		{
			auto skinnedMeshCom = node->GetComponent<COMSkinnedMesh>();
			auto meshData = skinnedMeshCom->GetMeshData();
			unsigned int numVertex = meshData->numData[MESHDATA_POSITION];

			ID3D12DescriptorHeap* heaps[] = { skinnedMeshCom->GetDescriptorHeap() };
			cmd->SetDescriptorHeaps(1, heaps);

			cmd->SetComputeRoot32BitConstant(ROOT_COMPUTEPASS_CONST, numVertex, 0);
			cmd->SetComputeRootDescriptorTable(ROOT_MESHDATA_TABLE, skinnedMeshCom->GetDescriptorHeap()->GetGPUDescriptorHandleForHeapStart());
			cmd->SetComputeRootShaderResourceView(ROOT_BONEDATA_SRV, skinnedMeshCom->GetBoneData(GetCurrFrameIndex()));

			cmd->Dispatch((numVertex / 32) + 1, 1, 1);

			cmd->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(skinnedMeshCom->GetResultMeshResource(),
				D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE));

			m_afterRenderEndResourceBarriers.emplace_back(CD3DX12_RESOURCE_BARRIER::Transition(skinnedMeshCom->GetResultMeshResource(),
				D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));
		};
}

void GraphicDeviceDX12::BuildDeferredSkinnedMeshPipeLineWorkSet()
{
	enum
	{
		ROOT_MAINPASS_CB = 0,
		ROOT_MATERIAL_CB,
		ROOT_TEXTURE_TABLE,
		ROOT_OBJECTINFO_CONST,
		ROOT_VERTEX_RESULT_SRV,
		ROOT_NORMAL_RESULT_SRV,
		ROOT_TANGENT_RESULT_SRV,
		ROOT_BITAN_RESULT_SRV,
		ROOT_NUM
	};

	std::string pipeLineName = "DeferredSkinnedMesh";
	auto& currPSOWorkSet = m_meshPSOWorkSets[pipeLineName];
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};

	{
		std::vector<D3D12_ROOT_PARAMETER> rootParams;
		rootParams.resize(ROOT_NUM);

		rootParams[ROOT_MAINPASS_CB].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		rootParams[ROOT_MAINPASS_CB].Descriptor.RegisterSpace = 0;
		rootParams[ROOT_MAINPASS_CB].Descriptor.ShaderRegister = 0;
		rootParams[ROOT_MAINPASS_CB].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

		rootParams[ROOT_MATERIAL_CB].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		rootParams[ROOT_MATERIAL_CB].Descriptor.RegisterSpace = 0;
		rootParams[ROOT_MATERIAL_CB].Descriptor.ShaderRegister = 1;
		rootParams[ROOT_MATERIAL_CB].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

		D3D12_DESCRIPTOR_RANGE textureTableRange[1] = {};
		textureTableRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		textureTableRange[0].RegisterSpace = 0;
		textureTableRange[0].BaseShaderRegister = 0;
		textureTableRange[0].NumDescriptors = CGHMaterial::CGHMaterialTextureNum;
		textureTableRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

		rootParams[ROOT_TEXTURE_TABLE].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		rootParams[ROOT_TEXTURE_TABLE].DescriptorTable.NumDescriptorRanges = 1;
		rootParams[ROOT_TEXTURE_TABLE].DescriptorTable.pDescriptorRanges = textureTableRange;
		rootParams[ROOT_TEXTURE_TABLE].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

		rootParams[ROOT_OBJECTINFO_CONST].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
		rootParams[ROOT_OBJECTINFO_CONST].Constants.Num32BitValues = 4;
		rootParams[ROOT_OBJECTINFO_CONST].Constants.RegisterSpace = 1;
		rootParams[ROOT_OBJECTINFO_CONST].Constants.ShaderRegister = 0;
		rootParams[ROOT_OBJECTINFO_CONST].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

		rootParams[ROOT_VERTEX_RESULT_SRV].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
		rootParams[ROOT_VERTEX_RESULT_SRV].Descriptor.RegisterSpace = 1;
		rootParams[ROOT_VERTEX_RESULT_SRV].Descriptor.ShaderRegister = 0;
		rootParams[ROOT_VERTEX_RESULT_SRV].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

		rootParams[ROOT_NORMAL_RESULT_SRV].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
		rootParams[ROOT_NORMAL_RESULT_SRV].Descriptor.RegisterSpace = 1;
		rootParams[ROOT_NORMAL_RESULT_SRV].Descriptor.ShaderRegister = 1;
		rootParams[ROOT_NORMAL_RESULT_SRV].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

		rootParams[ROOT_TANGENT_RESULT_SRV].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
		rootParams[ROOT_TANGENT_RESULT_SRV].Descriptor.RegisterSpace = 1;
		rootParams[ROOT_TANGENT_RESULT_SRV].Descriptor.ShaderRegister = 2;
		rootParams[ROOT_TANGENT_RESULT_SRV].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

		rootParams[ROOT_BITAN_RESULT_SRV].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
		rootParams[ROOT_BITAN_RESULT_SRV].Descriptor.RegisterSpace = 1;
		rootParams[ROOT_BITAN_RESULT_SRV].Descriptor.ShaderRegister = 3;
		rootParams[ROOT_BITAN_RESULT_SRV].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

		D3D12_ROOT_SIGNATURE_DESC rootsigDesc = {};
		rootsigDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
		rootsigDesc.pStaticSamplers = s_StaticSamplers;
		rootsigDesc.NumStaticSamplers = _countof(s_StaticSamplers);
		rootsigDesc.NumParameters = rootParams.size();
		rootsigDesc.pParameters = rootParams.data();

		psoDesc.pRootSignature = DX12PipelineMG::instance.CreateRootSignature(pipeLineName.c_str(), &rootsigDesc);
		currPSOWorkSet.rootSig = psoDesc.pRootSignature;
	}

	psoDesc.NodeMask = 0;

	psoDesc.VS = DX12PipelineMG::instance.CreateShader(DX12_SHADER_VERTEX, pipeLineName.c_str(), L"Shader/skinnedMeshShader.hlsl", "VS");
	psoDesc.HS = DX12PipelineMG::instance.CreateShader(DX12_SHADER_HULL, pipeLineName.c_str(), L"Shader/skinnedMeshShader.hlsl", "HS");
	psoDesc.DS = DX12PipelineMG::instance.CreateShader(DX12_SHADER_DOMAIN, pipeLineName.c_str(), L"Shader/skinnedMeshShader.hlsl", "DS");
	psoDesc.PS = DX12PipelineMG::instance.CreateShader(DX12_SHADER_PIXEL, pipeLineName.c_str(), L"Shader/skinnedMeshShader.hlsl", "PS");

	D3D12_INPUT_ELEMENT_DESC skinnedInputs[] = {
	{ "UV" ,0,DXGI_FORMAT_R32G32B32_FLOAT,0, 0,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0 },
	{ "UV" ,1,DXGI_FORMAT_R32G32B32_FLOAT,1,D3D12_APPEND_ALIGNED_ELEMENT,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0 },
	{ "UV" ,2,DXGI_FORMAT_R32G32B32_FLOAT,2,D3D12_APPEND_ALIGNED_ELEMENT,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0 } };

	//IA Set
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH;
	psoDesc.InputLayout.NumElements = _countof(skinnedInputs);
	psoDesc.InputLayout.pInputElementDescs = skinnedInputs;

	psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	//psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	//psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
	psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	psoDesc.SampleMask = UINT_MAX;

	psoDesc.BlendState.IndependentBlendEnable = true;

	psoDesc.BlendState.RenderTarget[DEFERRED_TEXTURE_HDR_DIFFUSE].BlendEnable = true;
	psoDesc.BlendState.RenderTarget[DEFERRED_TEXTURE_HDR_DIFFUSE].BlendOp = D3D12_BLEND_OP_ADD;
	psoDesc.BlendState.RenderTarget[DEFERRED_TEXTURE_HDR_DIFFUSE].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
	psoDesc.BlendState.RenderTarget[DEFERRED_TEXTURE_HDR_DIFFUSE].SrcBlend = D3D12_BLEND_SRC_ALPHA;
	psoDesc.BlendState.RenderTarget[DEFERRED_TEXTURE_HDR_DIFFUSE].BlendOpAlpha = D3D12_BLEND_OP_MAX;
	psoDesc.BlendState.RenderTarget[DEFERRED_TEXTURE_HDR_DIFFUSE].SrcBlend = D3D12_BLEND_ONE;
	psoDesc.BlendState.RenderTarget[DEFERRED_TEXTURE_HDR_DIFFUSE].DestBlendAlpha = D3D12_BLEND_ONE;

	psoDesc.BlendState.RenderTarget[DEFERRED_TEXTURE_RENDERID].BlendEnable = false;
	psoDesc.BlendState.RenderTarget[DEFERRED_TEXTURE_RENDERID].LogicOpEnable = false;

	//OM Set
	psoDesc.DSVFormat = m_swapChain->GetDSVFormat();
	psoDesc.NumRenderTargets = DEFERRED_TEXTURE_NUM;

	for (int i = 0; i < DEFERRED_TEXTURE_NUM; i++)
	{
		psoDesc.RTVFormats[i] = m_deferredFormat[i];
	}

	psoDesc.SampleDesc.Count = 1;
	psoDesc.SampleDesc.Quality = 0;

	currPSOWorkSet.pso = DX12PipelineMG::instance.CreateGraphicPipeline(pipeLineName.c_str(), &psoDesc);
	psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
	currPSOWorkSet.wireframePSO = DX12PipelineMG::instance.CreateGraphicPipeline((pipeLineName + "W").c_str(), &psoDesc);

	currPSOWorkSet.baseGraphicCmdFunc = [this](ID3D12GraphicsCommandList* cmd)
		{
			D3D12_CPU_DESCRIPTOR_HANDLE rtvs[DEFERRED_TEXTURE_NUM] = {};
			D3D12_CPU_DESCRIPTOR_HANDLE heapStart = m_deferredRTVHeap->GetCPUDescriptorHandleForHeapStart();

			rtvs[DEFERRED_TEXTURE_HDR_DIFFUSE] = m_swapChain->CurrRTV();
			for (int i = DEFERRED_TEXTURE_NORMAL; i < DEFERRED_TEXTURE_NUM; i++)
			{
				rtvs[i].ptr = heapStart.ptr + m_rtvSize * i;
			}

			cmd->OMSetRenderTargets(DEFERRED_TEXTURE_NUM, rtvs, false, &m_swapChain->GetDSV());
			cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_3_CONTROL_POINT_PATCHLIST);
			cmd->SetGraphicsRootConstantBufferView(ROOT_MAINPASS_CB, GetCurrMainPassCBV());
		};

	currPSOWorkSet.nodeGraphicCmdFunc = [this](ID3D12GraphicsCommandList* cmd, CGHNode* node)
		{
			COMMaterial* matCom = node->GetComponent<COMMaterial>();
			COMSkinnedMesh* meshCom = node->GetComponent<COMSkinnedMesh>();
			COMDX12SkinnedMeshRenderer* renderer = node->GetComponent<COMDX12SkinnedMeshRenderer>();
			unsigned int renderFlag = renderer->GetRenderFlags();

			D3D12_GPU_VIRTUAL_ADDRESS matCB = matCom->GetMaterialDataGPU(m_currFrame);
			const CGHMesh* currMesh = meshCom->GetMeshData();
			auto textureDescHeap = matCom->GetTextureHeap();
			auto descHeapHandle = textureDescHeap->GetGPUDescriptorHandleForHeapStart();
			bool isShadowGen = !(renderFlag & CGHRenderer::RENDER_FLAG_NON_TARGET_SHADOW);
			unsigned int id = renderer->GetRenderID();
			unsigned int numVertex = currMesh->numData[MESHDATA_POSITION];

			ID3D12DescriptorHeap* heaps[] = { textureDescHeap };
			cmd->SetDescriptorHeaps(_countof(heaps), heaps);

			D3D12_VERTEX_BUFFER_VIEW vbView[3] = {};

			unsigned int currUVChannel = min(static_cast<size_t>(3), currMesh->meshDataUVs.size());

			for (int i = 0; i < currUVChannel; i++)
			{
				vbView[i].BufferLocation = currMesh->meshDataUVs[i]->GetGPUVirtualAddress();
				vbView[i].SizeInBytes = sizeof(aiVector3D) * numVertex;
				vbView[i].StrideInBytes = sizeof(aiVector3D);
			}

			for (int i = currMesh->meshDataUVs.size(); i < 3; i++)
			{
				vbView[i].BufferLocation = currMesh->meshDataUVs[0]->GetGPUVirtualAddress();
				vbView[i].SizeInBytes = sizeof(aiVector3D) * numVertex;
				vbView[i].StrideInBytes = sizeof(aiVector3D);
			}

			UINT PhongTessAlpha = reinterpret_cast<UINT&>(GlobalOptions::GO.GRAPHIC.PhongTessAlpha);
			D3D12_INDEX_BUFFER_VIEW ibView = {};
			ibView.BufferLocation = currMesh->indices->GetGPUVirtualAddress();
			ibView.Format = DXGI_FORMAT_R32_UINT;
			ibView.SizeInBytes = sizeof(unsigned int) * currMesh->numData[MESHDATA_INDEX];

			cmd->IASetVertexBuffers(0, _countof(vbView), vbView);
			cmd->IASetVertexBuffers(0, 3, vbView);
			cmd->IASetIndexBuffer(&ibView);

			cmd->SetGraphicsRootConstantBufferView(ROOT_MATERIAL_CB, matCB);
			cmd->SetGraphicsRootDescriptorTable(ROOT_TEXTURE_TABLE, descHeapHandle);
			cmd->SetGraphicsRoot32BitConstant(ROOT_OBJECTINFO_CONST, id, 0);
			cmd->SetGraphicsRoot32BitConstant(ROOT_OBJECTINFO_CONST, isShadowGen, 1);
			cmd->SetGraphicsRoot32BitConstant(ROOT_OBJECTINFO_CONST, PhongTessAlpha, 2);
			cmd->SetGraphicsRoot32BitConstants(ROOT_OBJECTINFO_CONST, 1, &GlobalOptions::GO.GRAPHIC.PhongTessFactor, 3);

			cmd->SetGraphicsRootShaderResourceView(ROOT_VERTEX_RESULT_SRV, meshCom->GetResultMeshData(MESHDATA_POSITION));
			cmd->SetGraphicsRootShaderResourceView(ROOT_NORMAL_RESULT_SRV, meshCom->GetResultMeshData(MESHDATA_NORMAL));
			cmd->SetGraphicsRootShaderResourceView(ROOT_TANGENT_RESULT_SRV, meshCom->GetResultMeshData(MESHDATA_TAN));
			cmd->SetGraphicsRootShaderResourceView(ROOT_BITAN_RESULT_SRV, meshCom->GetResultMeshData(MESHDATA_BITAN));

			cmd->DrawIndexedInstanced(currMesh->numData[MESHDATA_INDEX], 1, 0, 0, 0);

			if (!(renderFlag & CGHRenderer::RENDER_FLAG_NON_TARGET_SHADOW))
			{
				DX12SahdowGenMesh shadowgen;
				shadowgen.vertexBuffer = meshCom->GetResultMeshResource();
				shadowgen.indicesBuffer = currMesh->indices;
				shadowgen.numIndices = currMesh->numData[MESHDATA_INDEX];
				m_shadowGenMeshs.emplace_back(shadowgen);
			}
		};
}

void GraphicDeviceDX12::BuildShadowMapWritePipeLineWorkSet()
{
	enum
	{
		ROOT_OBEJCTTRANSFORM_CONST,
		ROOT_SHADOWPASS_CB,
		ROOT_VERTEX_SRV,
		ROOT_NUM
	};

	std::string pipeLineName = "dirShadowWrite";
	auto& currPSOWorkSet = m_basePSOWorkSets[PSOW_SHADOWMAP_WRITE];
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};

	{
		std::vector<D3D12_ROOT_PARAMETER> rootParams(ROOT_NUM);

		rootParams[ROOT_OBEJCTTRANSFORM_CONST].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
		rootParams[ROOT_OBEJCTTRANSFORM_CONST].Constants.RegisterSpace = 0;
		rootParams[ROOT_OBEJCTTRANSFORM_CONST].Constants.ShaderRegister = 0;
		rootParams[ROOT_OBEJCTTRANSFORM_CONST].Constants.Num32BitValues = 16;
		rootParams[ROOT_OBEJCTTRANSFORM_CONST].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

		rootParams[ROOT_SHADOWPASS_CB].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		rootParams[ROOT_SHADOWPASS_CB].Descriptor.RegisterSpace = 0;
		rootParams[ROOT_SHADOWPASS_CB].Descriptor.ShaderRegister = 1;
		rootParams[ROOT_SHADOWPASS_CB].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

		rootParams[ROOT_VERTEX_SRV].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
		rootParams[ROOT_VERTEX_SRV].Descriptor.RegisterSpace = 0;
		rootParams[ROOT_VERTEX_SRV].Descriptor.ShaderRegister = 0;
		rootParams[ROOT_VERTEX_SRV].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

		D3D12_ROOT_SIGNATURE_DESC rootsigDesc = {};
		rootsigDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
		rootsigDesc.pStaticSamplers = nullptr;
		rootsigDesc.NumStaticSamplers = 0;
		rootsigDesc.NumParameters = rootParams.size();
		rootsigDesc.pParameters = rootParams.data();

		psoDesc.pRootSignature = DX12PipelineMG::instance.CreateRootSignature(pipeLineName.c_str(), &rootsigDesc);
		currPSOWorkSet.rootSig = psoDesc.pRootSignature;
	}

	psoDesc.NodeMask = 0;
	psoDesc.VS = DX12PipelineMG::instance.CreateShader(DX12_SHADER_VERTEX, pipeLineName.c_str(), L"Shader/ShadowMapGenShader.hlsl", "VS");

	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.InputLayout.NumElements = 0;
	psoDesc.InputLayout.pInputElementDescs = nullptr;

	psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	psoDesc.RasterizerState.DepthBias = 100000;
	psoDesc.RasterizerState.DepthBiasClamp = 0.0f;
	psoDesc.RasterizerState.SlopeScaledDepthBias = 1.0f;

	psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	psoDesc.SampleMask = UINT_MAX;

	psoDesc.DSVFormat = m_swapChain->GetDSVFormat();
	psoDesc.NumRenderTargets = 0;
	psoDesc.SampleDesc.Count = 1;
	psoDesc.SampleDesc.Quality = 0;

	currPSOWorkSet.pso = DX12PipelineMG::instance.CreateGraphicPipeline(pipeLineName.c_str(), &psoDesc);
	currPSOWorkSet.baseGraphicCmdFunc = [this](ID3D12GraphicsCommandList* cmd)
		{
			auto lights = m_renderQueues[RENDERQUEUE_LIGHT_DIR].queue;

			for (m_numDirLight = 0; m_numDirLight < lights.size(); ++m_numDirLight)
			{
				auto lightCom = lights[m_numDirLight].first->GetComponent<COMDirLight>();

				if (lightCom)
				{
					if (lights[m_numDirLight].second & CGHLightComponent::LIGHT_FLAG_SHADOW)
					{
						auto& shadowMap = m_dirLightShadowMaps[lightCom];
						float radius = (m_cameraDistance + 2.0f) * 1.5f;
						XMVECTOR lightDir = XMLoadFloat3(&lightCom->m_data.dir);
						XMVECTOR targetPos = XMLoadFloat3(&m_cameraTargetPos);
						XMVECTOR lightPos = targetPos - (radius * lightDir);
						XMVECTOR lightUp = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
						XMMATRIX lightView = XMMatrixLookAtLH(lightPos, targetPos, lightUp);

						XMFLOAT3 sphereCenterLS;
						XMStoreFloat3(&sphereCenterLS, XMVector3TransformCoord(targetPos, lightView));

						float l = sphereCenterLS.x - radius;
						float b = sphereCenterLS.y - radius;
						float n = sphereCenterLS.z - radius;
						float r = sphereCenterLS.x + radius;
						float t = sphereCenterLS.y + radius;
						float f = sphereCenterLS.z + radius;

						XMMATRIX lightProj = XMMatrixOrthographicOffCenterLH(l, r, b, t, n, f);

						XMMATRIX viewProj = lightView * lightProj;

						XMStoreFloat4x4(&shadowMap.lightViewProj, viewProj);

						m_shadowPassCB->CopyData(m_numMaxShadowMap * m_currFrame + shadowMap.srvHeapIndex, &XMMatrixTranspose(viewProj));
					}
				}
			}

			for (auto& iter : lights)
			{
				if (iter.second & CGHLightComponent::LIGHT_FLAG_SHADOW)
				{
					auto* lightCom = iter.first->GetComponent<COMDirLight>();
					if (lightCom)
					{
						auto shadowMap = m_dirLightShadowMaps[lightCom];
						auto shadowMapDSV = shadowMap.texDSVHeap->GetCPUDescriptorHandleForHeapStart();

						cmd->ClearDepthStencilView(shadowMapDSV,
							D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);
						cmd->OMSetRenderTargets(0, nullptr, false, &shadowMapDSV);

						auto passCBV = m_shadowPassCB->Resource()->GetGPUVirtualAddress();
						passCBV += (m_numMaxShadowMap * m_currFrame + shadowMap.srvHeapIndex) * m_shadowPassCB->GetElementByteSize();
						cmd->SetGraphicsRootConstantBufferView(ROOT_SHADOWPASS_CB, passCBV);

						cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

						for (auto& currMesh : m_shadowGenMeshs)
						{
							D3D12_INDEX_BUFFER_VIEW ibv = {};
							ibv.BufferLocation = currMesh.indicesBuffer->GetGPUVirtualAddress();
							ibv.Format = DXGI_FORMAT_R32_UINT;
							ibv.SizeInBytes = sizeof(unsigned int) * currMesh.numIndices;

							cmd->IASetIndexBuffer(&ibv);
							cmd->SetGraphicsRoot32BitConstants(ROOT_OBEJCTTRANSFORM_CONST, 16, &currMesh.transformMat, 0);
							cmd->SetGraphicsRootShaderResourceView(ROOT_VERTEX_SRV, currMesh.vertexBuffer->GetGPUVirtualAddress());
							cmd->DrawIndexedInstanced(currMesh.numIndices, 1, 0, 0, 0);
						}

						D3D12_RESOURCE_BARRIER resourceBar = CD3DX12_RESOURCE_BARRIER::Transition(shadowMap.resource.Get(),
							D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_GENERIC_READ);
						cmd->ResourceBarrier(1, &resourceBar);

						resourceBar.Transition.StateBefore = D3D12_RESOURCE_STATE_GENERIC_READ;
						resourceBar.Transition.StateAfter = D3D12_RESOURCE_STATE_DEPTH_WRITE;
						m_afterRenderEndResourceBarriers.emplace_back(resourceBar);
					}
				}
			}

			m_shadowGenMeshs.clear();
		};
}

void GraphicDeviceDX12::BuildDeferredLightDirPipeLineWorkSet()
{
	enum
	{
		ROOT_MAINPASS_CB = 0,
		ROOT_TEXTURE_TABLE,
		ROOT_LIGHTOPTION_CONST,
		ROOT_LIGHTDATA_SRV,
		ROOT_NUM
	};

	std::string pipeLineName = "DeferredLightDir";
	auto& currPSOWorkSet = m_basePSOWorkSets[PSOW_DEFERRED_LIGHT_DIR];
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};

	{
		std::vector<D3D12_ROOT_PARAMETER> rootParams(ROOT_NUM);

		rootParams[ROOT_MAINPASS_CB].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		rootParams[ROOT_MAINPASS_CB].Descriptor.RegisterSpace = 0;
		rootParams[ROOT_MAINPASS_CB].Descriptor.ShaderRegister = 0;
		rootParams[ROOT_MAINPASS_CB].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

		CD3DX12_DESCRIPTOR_RANGE shadowTexTableRange[2];
		shadowTexTableRange[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, DEFERRED_TEXTURE_NUM, 0);
		shadowTexTableRange[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, m_numMaxShadowMap, 0, 1);

		rootParams[ROOT_TEXTURE_TABLE].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		rootParams[ROOT_TEXTURE_TABLE].DescriptorTable.NumDescriptorRanges = 2;
		rootParams[ROOT_TEXTURE_TABLE].DescriptorTable.pDescriptorRanges = shadowTexTableRange;
		rootParams[ROOT_TEXTURE_TABLE].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

		rootParams[ROOT_LIGHTOPTION_CONST].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
		rootParams[ROOT_LIGHTOPTION_CONST].Constants.RegisterSpace = 0;
		rootParams[ROOT_LIGHTOPTION_CONST].Constants.ShaderRegister = 1;
		rootParams[ROOT_LIGHTOPTION_CONST].Constants.Num32BitValues = 1;
		rootParams[ROOT_LIGHTOPTION_CONST].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

		rootParams[ROOT_LIGHTDATA_SRV].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
		rootParams[ROOT_LIGHTDATA_SRV].Descriptor.RegisterSpace = 0;
		rootParams[ROOT_LIGHTDATA_SRV].Descriptor.ShaderRegister = 5;
		rootParams[ROOT_LIGHTDATA_SRV].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

		D3D12_ROOT_SIGNATURE_DESC rootsigDesc = {};
		rootsigDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
		rootsigDesc.pStaticSamplers = s_StaticSamplers;
		rootsigDesc.NumStaticSamplers = _countof(s_StaticSamplers);
		rootsigDesc.NumParameters = rootParams.size();
		rootsigDesc.pParameters = rootParams.data();

		psoDesc.pRootSignature = DX12PipelineMG::instance.CreateRootSignature(pipeLineName.c_str(), &rootsigDesc);
		currPSOWorkSet.rootSig = psoDesc.pRootSignature;
	}

	psoDesc.NodeMask = 0;
	psoDesc.pRootSignature = DX12PipelineMG::instance.GetRootSignature(pipeLineName.c_str());

	psoDesc.VS = DX12PipelineMG::instance.CreateShader(DX12_SHADER_VERTEX, pipeLineName.c_str(), L"Shader/DeferredLightDir.hlsl", "VS");
	psoDesc.GS = DX12PipelineMG::instance.CreateShader(DX12_SHADER_GEOMETRY, pipeLineName.c_str(), L"Shader/DeferredLightDir.hlsl", "GS");
	psoDesc.PS = DX12PipelineMG::instance.CreateShader(DX12_SHADER_PIXEL, pipeLineName.c_str(), L"Shader/DeferredLightDir.hlsl", "PS");

	//IA Set
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
	psoDesc.InputLayout.NumElements = 0;
	psoDesc.InputLayout.pInputElementDescs = nullptr;

	psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	psoDesc.SampleMask = UINT_MAX;

	psoDesc.BlendState.RenderTarget[0].BlendEnable = true;
	psoDesc.BlendState.RenderTarget[0].LogicOpEnable = false;
	psoDesc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
	psoDesc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_DEST_ALPHA;
	psoDesc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	psoDesc.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
	psoDesc.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ONE;
	psoDesc.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_MAX;
	psoDesc.BlendState.RenderTarget[0].LogicOp = D3D12_LOGIC_OP_NOOP;
	psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	psoDesc.DepthStencilState.DepthEnable = false;
	psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_GREATER_EQUAL;

	psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	psoDesc.RasterizerState.DepthClipEnable = false;

	//OM Set
	psoDesc.DSVFormat = m_swapChain->GetDSVFormat();
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = m_backBufferFormat;
	psoDesc.SampleDesc.Count = 1;
	psoDesc.SampleDesc.Quality = 0;

	currPSOWorkSet.pso = DX12PipelineMG::instance.CreateGraphicPipeline(pipeLineName.c_str(), &psoDesc);
	currPSOWorkSet.baseGraphicCmdFunc = [this](ID3D12GraphicsCommandList* cmd)
		{
			auto lights = m_renderQueues[RENDERQUEUE_LIGHT_DIR].queue;

			for (m_numDirLight = 0; m_numDirLight < lights.size(); ++m_numDirLight)
			{
				auto lightCom = lights[m_numDirLight].first->GetComponent<COMDirLight>();

				if (lightCom)
				{
					DX12DirLightData lightData;
					lightData.color = lightCom->m_data.color;
					lightData.power = lightCom->m_data.power;
					lightData.dir = lightCom->m_data.dir;
					lightData.enableShadow = lights[m_numDirLight].second & CGHLightComponent::LIGHT_FLAG_SHADOW;

					if (lightData.enableShadow)
					{
						auto& shadowMap = m_dirLightShadowMaps[lightCom];

						lightData.shadowMapIndex = shadowMap.srvHeapIndex;

						const static XMMATRIX T(
							0.5f, 0.0f, 0.0f, 0.0f,
							0.0f, -0.5f, 0.0f, 0.0f,
							0.0f, 0.0f, 1.0f, 0.0f,
							0.5f, 0.5f, 0.0f, 1.0f);

						XMMATRIX lightViewProj = XMLoadFloat4x4(&shadowMap.lightViewProj);

						XMStoreFloat4x4(&lightData.shadowMapMat, XMMatrixTranspose(lightViewProj * T));
					}

					m_dirLightDatas->CopyData((m_numMaxDirLight * m_currFrame) + m_numDirLight, lightData);
				}
			}

			CD3DX12_RESOURCE_BARRIER resourceBars[DEFERRED_TEXTURE_NUM + 1] = {};
			{
				resourceBars[DEFERRED_TEXTURE_HDR_DIFFUSE] = CD3DX12_RESOURCE_BARRIER::Transition(m_swapChain->GetCurrRenderTargetResource(),
					D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ);

				for (int i = 1; i < DEFERRED_TEXTURE_NUM; i++)
				{
					resourceBars[i] = CD3DX12_RESOURCE_BARRIER::Transition(m_deferredResources[i].Get(),
						D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ);
				}

				resourceBars[DEFERRED_TEXTURE_NUM] = CD3DX12_RESOURCE_BARRIER::Transition(m_swapChain->GetDSResource(),
					D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_GENERIC_READ);

			}
			cmd->ResourceBarrier(_countof(resourceBars), resourceBars);

			D3D12_CPU_DESCRIPTOR_HANDLE rtv = m_diffuseTextureRTVHeap->GetCPUDescriptorHandleForHeapStart();

			cmd->OMSetRenderTargets(1, &rtv, false, nullptr);
			cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_POINTLIST);
			cmd->IASetVertexBuffers(0, 0, nullptr);
			auto srvHeapIter = m_lightRenderSRVHeaps.find(m_swapChain->GetCurrRenderTargetResource());
			assert(srvHeapIter != m_lightRenderSRVHeaps.end());
			ID3D12DescriptorHeap* heaps[] = { srvHeapIter->second.Get() };
			cmd->SetDescriptorHeaps(1, heaps);

			auto ligthDataStart = m_dirLightDatas->Resource()->GetGPUVirtualAddress();
			ligthDataStart += (m_numMaxDirLight * m_currFrame) * m_dirLightDatas->GetElementByteSize();

			cmd->SetGraphicsRootConstantBufferView(ROOT_MAINPASS_CB, GetCurrMainPassCBV());
			cmd->SetGraphicsRootDescriptorTable(ROOT_TEXTURE_TABLE, srvHeapIter->second->GetGPUDescriptorHandleForHeapStart());
			cmd->SetGraphicsRoot32BitConstant(ROOT_LIGHTOPTION_CONST, m_numDirLight, 0);
			cmd->SetGraphicsRootShaderResourceView(ROOT_LIGHTDATA_SRV, ligthDataStart);

			cmd->DrawInstanced(1, 1, 0, 0);

			cmd->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_swapChain->GetDSResource(),
				D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_DEPTH_WRITE));
		};
}

void GraphicDeviceDX12::BuildSMAARenderPipeLineWorkSet()
{
	enum
	{
		ROOT_SMAA_PASS_CONST = 0,
		ROOT_SMAA_RESORUCE_TABLE,
		ROOT_NUM
	};

	std::string pipeLineName = "SMAA";
	auto currPSOWorkSet = &m_basePSOWorkSets[PSOW_SMAA_EDGE_RENDER];
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
	{
		std::vector<D3D12_ROOT_PARAMETER> rootParams(ROOT_NUM);
		rootParams[ROOT_SMAA_PASS_CONST].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
		rootParams[ROOT_SMAA_PASS_CONST].Constants.Num32BitValues = 12;
		rootParams[ROOT_SMAA_PASS_CONST].Constants.RegisterSpace = 0;
		rootParams[ROOT_SMAA_PASS_CONST].Constants.ShaderRegister = 0;
		rootParams[ROOT_SMAA_PASS_CONST].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

		D3D12_DESCRIPTOR_RANGE textureTableRange = {};
		textureTableRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		textureTableRange.RegisterSpace = 0;
		textureTableRange.BaseShaderRegister = 0;
		textureTableRange.NumDescriptors = 6;
		textureTableRange.OffsetInDescriptorsFromTableStart = 0;

		rootParams[ROOT_SMAA_RESORUCE_TABLE].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		rootParams[ROOT_SMAA_RESORUCE_TABLE].DescriptorTable.NumDescriptorRanges = 1;
		rootParams[ROOT_SMAA_RESORUCE_TABLE].DescriptorTable.pDescriptorRanges = &textureTableRange;
		rootParams[ROOT_SMAA_RESORUCE_TABLE].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

		D3D12_ROOT_SIGNATURE_DESC rootsigDesc = {};
		rootsigDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
		rootsigDesc.pStaticSamplers = s_StaticSamplers;
		rootsigDesc.NumStaticSamplers = _countof(s_StaticSamplers);
		rootsigDesc.NumParameters = rootParams.size();
		rootsigDesc.pParameters = rootParams.data();

		psoDesc.pRootSignature = DX12PipelineMG::instance.CreateRootSignature(pipeLineName.c_str(), &rootsigDesc);
	}

	psoDesc.NodeMask = 0;
	psoDesc.SampleDesc.Count = 1;
	psoDesc.SampleDesc.Quality = 0;

	//IA Set
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
	psoDesc.InputLayout.NumElements = 0;
	psoDesc.InputLayout.pInputElementDescs = nullptr;

	{
		psoDesc.VS = DX12PipelineMG::instance.CreateShader(DX12_SHADER_VERTEX, (pipeLineName).c_str(), L"Shader/SMAAShader.hlsl", "VS");
		psoDesc.GS = DX12PipelineMG::instance.CreateShader(DX12_SHADER_GEOMETRY, (pipeLineName + "EDGE").c_str(), L"Shader/SMAAShader.hlsl", "EdgeGS");
		psoDesc.PS = DX12PipelineMG::instance.CreateShader(DX12_SHADER_PIXEL, (pipeLineName + "EDGE").c_str(), L"Shader/SMAAShader.hlsl", "EdgePS");

		psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);

		psoDesc.DepthStencilState.DepthEnable = false;
		psoDesc.DepthStencilState.StencilEnable = true;
		psoDesc.DepthStencilState.FrontFace.StencilPassOp = D3D12_STENCIL_OP_REPLACE;
		psoDesc.SampleMask = UINT_MAX;

		psoDesc.BlendState.RenderTarget[0].BlendEnable = false;

		psoDesc.DSVFormat = m_swapChain->GetDSVFormat();
		psoDesc.NumRenderTargets = 1;
		psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8_UNORM;

		currPSOWorkSet->pso = DX12PipelineMG::instance.CreateGraphicPipeline((pipeLineName + "EDGE").c_str(), &psoDesc);
		currPSOWorkSet->rootSig = DX12PipelineMG::instance.GetRootSignature(pipeLineName.c_str());

		currPSOWorkSet->baseGraphicCmdFunc = [this](ID3D12GraphicsCommandList* cmd)
			{
				if (GlobalOptions::GO.GRAPHIC.EnableSMAA)
				{
					XMVECTOR vecZero = XMVectorZero();
					cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_POINTLIST);
					cmd->ClearRenderTargetView(m_smaa->GetEdgeRenderTarget(), vecZero.m128_f32, 0, nullptr);
					cmd->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_deferredResources[DEFERRED_TEXTURE_HDR_DIFFUSE].Get(),
						D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ));

					ID3D12DescriptorHeap* heaps[] = { m_smaa->GetSRVHeap() };
					cmd->SetDescriptorHeaps(1, heaps);
					cmd->OMSetRenderTargets(1, &m_smaa->GetEdgeRenderTarget(), true, &m_swapChain->GetDSV());
					cmd->OMSetStencilRef(1);

					XMVECTOR windowInfo = XMVectorSet(1.0f / GlobalOptions::GO.WIN.WindowsizeX, 1.0f / GlobalOptions::GO.WIN.WindowsizeY,
						GlobalOptions::GO.WIN.WindowsizeX, GlobalOptions::GO.WIN.WindowsizeY);

					cmd->SetGraphicsRoot32BitConstants(ROOT_SMAA_PASS_CONST, 4, &windowInfo, 0);
					cmd->SetGraphicsRoot32BitConstants(ROOT_SMAA_PASS_CONST, 4, &vecZero, 4);
					cmd->SetGraphicsRoot32BitConstants(ROOT_SMAA_PASS_CONST, 4, &GlobalOptions::GO.GRAPHIC.SMAAEdgeDetectionThreshold, 8);
					cmd->SetGraphicsRootDescriptorTable(ROOT_SMAA_RESORUCE_TABLE, m_smaa->GetSRVHeap()->GetGPUDescriptorHandleForHeapStart());

					cmd->DrawInstanced(1, 1, 0, 0);
				}
			};

		currPSOWorkSet->nodeGraphicCmdFunc = nullptr;
	}

	{
		currPSOWorkSet = &m_basePSOWorkSets[PSOW_SMAA_BLEND_RENDER];

		psoDesc.GS = DX12PipelineMG::instance.CreateShader(DX12_SHADER_GEOMETRY, (pipeLineName + "BLEND").c_str(), L"Shader/SMAAShader.hlsl", "BlendGS");
		psoDesc.PS = DX12PipelineMG::instance.CreateShader(DX12_SHADER_PIXEL, (pipeLineName + "BLEND").c_str(), L"Shader/SMAAShader.hlsl", "BlendPS");

		psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);

		psoDesc.DepthStencilState.DepthEnable = false;
		psoDesc.DepthStencilState.StencilEnable = true;
		psoDesc.DepthStencilState.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_EQUAL;
		psoDesc.SampleMask = UINT_MAX;

		psoDesc.BlendState.RenderTarget[0].BlendEnable = false;

		psoDesc.DSVFormat = m_swapChain->GetDSVFormat();
		psoDesc.NumRenderTargets = 1;
		psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;

		currPSOWorkSet->pso = DX12PipelineMG::instance.CreateGraphicPipeline((pipeLineName + "BLEND").c_str(), &psoDesc);
		currPSOWorkSet->rootSig = DX12PipelineMG::instance.GetRootSignature(pipeLineName.c_str());

		currPSOWorkSet->baseGraphicCmdFunc = [this](ID3D12GraphicsCommandList* cmd)
			{
				if (GlobalOptions::GO.GRAPHIC.EnableSMAA)
				{
					XMVECTOR vecZero = XMVectorZero();
					cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_POINTLIST);
					cmd->ClearRenderTargetView(m_smaa->GetBlendRenderTarget(), vecZero.m128_f32, 0, nullptr);

					ID3D12DescriptorHeap* heaps[] = { m_smaa->GetSRVHeap() };
					cmd->SetDescriptorHeaps(1, heaps);
					cmd->OMSetRenderTargets(1, &m_smaa->GetBlendRenderTarget(), false, &m_swapChain->GetDSV());
					cmd->OMSetStencilRef(1);

					XMVECTOR windowInfo = XMVectorSet(1.0f / GlobalOptions::GO.WIN.WindowsizeX, 1.0f / GlobalOptions::GO.WIN.WindowsizeY,
						GlobalOptions::GO.WIN.WindowsizeX, GlobalOptions::GO.WIN.WindowsizeY);

					cmd->SetGraphicsRoot32BitConstants(ROOT_SMAA_PASS_CONST, 4, &windowInfo, 0);
					cmd->SetGraphicsRoot32BitConstants(ROOT_SMAA_PASS_CONST, 4, &vecZero, 4);
					cmd->SetGraphicsRoot32BitConstants(ROOT_SMAA_PASS_CONST, 4, &GlobalOptions::GO.GRAPHIC.SMAAEdgeDetectionThreshold, 8);
					cmd->SetGraphicsRootDescriptorTable(ROOT_SMAA_RESORUCE_TABLE, m_smaa->GetSRVHeap()->GetGPUDescriptorHandleForHeapStart());

					cmd->DrawInstanced(1, 1, 0, 0);
				}
			};
	}

	{
		currPSOWorkSet = &m_basePSOWorkSets[PSOW_SMAA_NEIBLEND_RENDER];

		psoDesc.GS = DX12PipelineMG::instance.CreateShader(DX12_SHADER_GEOMETRY, (pipeLineName + "NEIBLEND").c_str(), L"Shader/SMAAShader.hlsl", "NeiBlendGS");
		psoDesc.PS = DX12PipelineMG::instance.CreateShader(DX12_SHADER_PIXEL, (pipeLineName + "NEIBLEND").c_str(), L"Shader/SMAAShader.hlsl", "NeiBlendPS");

		psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);

		psoDesc.DepthStencilState.DepthEnable = false;
		psoDesc.DepthStencilState.StencilEnable = false;
		psoDesc.SampleMask = UINT_MAX;

		psoDesc.BlendState.AlphaToCoverageEnable = true;
		psoDesc.BlendState.RenderTarget[0].BlendEnable = false;
		psoDesc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
		psoDesc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
		psoDesc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
		psoDesc.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_MAX;
		psoDesc.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
		psoDesc.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ONE;

		psoDesc.DSVFormat = DXGI_FORMAT_UNKNOWN;
		psoDesc.NumRenderTargets = 1;
		psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;

		currPSOWorkSet->pso = DX12PipelineMG::instance.CreateGraphicPipeline((pipeLineName + "NEIBLEND").c_str(), &psoDesc);
		currPSOWorkSet->rootSig = DX12PipelineMG::instance.GetRootSignature(pipeLineName.c_str());

		currPSOWorkSet->baseGraphicCmdFunc = [this](ID3D12GraphicsCommandList* cmd)
			{
				if (GlobalOptions::GO.GRAPHIC.EnableSMAA)
				{
					XMVECTOR vecZero = XMVectorZero();
					cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_POINTLIST);
					cmd->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_smaaResult.Get(),
						D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET));


					ID3D12DescriptorHeap* heaps[] = { m_smaa->GetSRVHeap() };
					cmd->SetDescriptorHeaps(1, heaps);
					cmd->OMSetRenderTargets(1, &GetSMAAResultRTV(), true, nullptr);
					cmd->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_smaa->GetBlendTex(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));

					XMVECTOR windowInfo = XMVectorSet(1.0f / GlobalOptions::GO.WIN.WindowsizeX, 1.0f / GlobalOptions::GO.WIN.WindowsizeY,
						GlobalOptions::GO.WIN.WindowsizeX, GlobalOptions::GO.WIN.WindowsizeY);

					cmd->SetGraphicsRoot32BitConstants(ROOT_SMAA_PASS_CONST, 4, &windowInfo, 0);
					cmd->SetGraphicsRoot32BitConstants(ROOT_SMAA_PASS_CONST, 4, &vecZero, 4);
					cmd->SetGraphicsRoot32BitConstants(ROOT_SMAA_PASS_CONST, 4, &GlobalOptions::GO.GRAPHIC.SMAAEdgeDetectionThreshold, 8);
					cmd->SetGraphicsRootDescriptorTable(ROOT_SMAA_RESORUCE_TABLE, m_smaa->GetSRVHeap()->GetGPUDescriptorHandleForHeapStart());

					cmd->DrawInstanced(1, 1, 0, 0);

					cmd->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_smaa->GetBlendTex(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET));
				}
			};
	}
}

void GraphicDeviceDX12::BuildPostProcessingPipeLineWorkSet()
{
}

void GraphicDeviceDX12::BuildTextureDataDebugPipeLineWorkSet()
{
	enum
	{
		ROOT_WINSIZE_CONST = 0,
		ROOT_DEBUG_TEXTURE_TABLE,
		ROOT_NUM
	};

	std::string pipeLineName = "textureDebug";
	auto& currPSOWorkSet = m_basePSOWorkSets[PSOW_TEX_DEBUG];
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};

	{
		std::vector<D3D12_ROOT_PARAMETER> rootParams(ROOT_NUM);

		rootParams[ROOT_WINSIZE_CONST].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
		rootParams[ROOT_WINSIZE_CONST].Constants.Num32BitValues = 2;
		rootParams[ROOT_WINSIZE_CONST].Constants.RegisterSpace = 0;
		rootParams[ROOT_WINSIZE_CONST].Constants.ShaderRegister = 0;
		rootParams[ROOT_WINSIZE_CONST].ShaderVisibility = D3D12_SHADER_VISIBILITY_GEOMETRY;

		CD3DX12_DESCRIPTOR_RANGE debugTextureRange;
		debugTextureRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 6, 0);

		rootParams[ROOT_DEBUG_TEXTURE_TABLE].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		rootParams[ROOT_DEBUG_TEXTURE_TABLE].DescriptorTable.NumDescriptorRanges = 1;
		rootParams[ROOT_DEBUG_TEXTURE_TABLE].DescriptorTable.pDescriptorRanges = &debugTextureRange;
		rootParams[ROOT_DEBUG_TEXTURE_TABLE].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

		D3D12_ROOT_SIGNATURE_DESC rootsigDesc = {};
		rootsigDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
		rootsigDesc.pStaticSamplers = s_StaticSamplers;
		rootsigDesc.NumStaticSamplers = _countof(s_StaticSamplers);
		rootsigDesc.NumParameters = rootParams.size();
		rootsigDesc.pParameters = rootParams.data();

		psoDesc.pRootSignature = DX12PipelineMG::instance.CreateRootSignature(pipeLineName.c_str(), &rootsigDesc);
		currPSOWorkSet.rootSig = psoDesc.pRootSignature;
	}

	psoDesc.NodeMask = 0;

	psoDesc.VS = DX12PipelineMG::instance.CreateShader(DX12_SHADER_VERTEX, (pipeLineName).c_str(), L"Shader/TextureDebugShader.hlsl", "VS");
	psoDesc.GS = DX12PipelineMG::instance.CreateShader(DX12_SHADER_GEOMETRY, (pipeLineName).c_str(), L"Shader/TextureDebugShader.hlsl", "GS");
	psoDesc.PS = DX12PipelineMG::instance.CreateShader(DX12_SHADER_PIXEL, (pipeLineName).c_str(), L"Shader/TextureDebugShader.hlsl", "PS");

	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
	psoDesc.InputLayout.NumElements = 0;
	psoDesc.InputLayout.pInputElementDescs = nullptr;

	psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	psoDesc.DepthStencilState.DepthEnable = false;
	psoDesc.SampleMask = UINT_MAX;

	psoDesc.DSVFormat = DXGI_FORMAT_UNKNOWN;
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = m_backBufferFormat;
	psoDesc.SampleDesc.Count = 1;
	psoDesc.SampleDesc.Quality = 0;

	currPSOWorkSet.pso = DX12PipelineMG::instance.CreateGraphicPipeline(pipeLineName.c_str(), &psoDesc);

	currPSOWorkSet.baseGraphicCmdFunc = [this](ID3D12GraphicsCommandList* cmd)
		{
			if (GlobalOptions::GO.GRAPHIC.EnableTextureDebugPSO)
			{
				auto rtv = m_swapChain->CurrRTV();
				cmd->OMSetRenderTargets(1, &rtv, false, nullptr);

				cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_POINTLIST);

				ID3D12DescriptorHeap* descHeaps[] = { m_smaa->GetSRVHeap() };
				cmd->SetDescriptorHeaps(1, descHeaps);
				cmd->SetGraphicsRoot32BitConstant(ROOT_WINSIZE_CONST, GlobalOptions::GO.WIN.WindowsizeX, 0);
				cmd->SetGraphicsRoot32BitConstant(ROOT_WINSIZE_CONST, GlobalOptions::GO.WIN.WindowsizeY, 1);

				auto srvHeap = m_smaa->GetSRVHeap()->GetGPUDescriptorHandleForHeapStart();
				cmd->SetGraphicsRootDescriptorTable(ROOT_DEBUG_TEXTURE_TABLE, srvHeap);

				cmd->DrawInstanced(1, 1, 0, 0);
			}
		};
}

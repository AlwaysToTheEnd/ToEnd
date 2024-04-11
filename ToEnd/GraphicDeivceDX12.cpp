#include "GraphicDeivceDX12.h"
#include "../Common/Source/CGHUtil.h"
#include "../Common/Source/DxException.h"
#include "../Common/Source/DX12SwapChain.h"

#include "GraphicResourceLoader.h"
#include "DX12GarbageFrameResourceMG.h"
#include "DX12PipelineMG.h"
#include "BaseComponents.h"
#include "LightComponents.h"
#include "Camera.h"
#include "Mouse.h"

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
	auto& currPso = m_PSOs[PIPELINE_SKINNEDMESH];

	if (currPso.pso)
	{
		m_cmdList->SetPipelineState(currPso.pso);
		m_cmdList->SetGraphicsRootSignature(currPso.rootSig);

		currPso.baseGraphicCmdFunc(m_cmdList.Get());

		for (auto& currNode : m_skinnedMeshRenderQueue.queue)
		{
			currPso.nodeGraphicCmdFunc(m_cmdList.Get(), currNode.first, currNode.second);
		}
	}
}

void GraphicDeviceDX12::LightRender()
{
	auto presentDSV = m_swapChain->GetDSV();
	D3D12_CPU_DESCRIPTOR_HANDLE rtvs[] = { m_swapChain->CurrRTV() };

	m_cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_swapChain->GetDSResource(),
		D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_GENERIC_READ));
	m_cmdList->OMSetRenderTargets(1, rtvs, true, nullptr);

	{
		auto& currPso = m_lightPSOs[PIPELINE_LIGHT_DIR];

		if (currPso.pso != nullptr)
		{
			m_cmdList->SetPipelineState(currPso.pso);
			m_cmdList->SetGraphicsRootSignature(currPso.rootSig);

			currPso.baseGraphicCmdFunc(m_cmdList.Get());

			auto currQeue = currPso.renderQueue;
			if (currQeue)
			{
				m_numDirLight = 0;
				unsigned int maxLightNum = m_dirLightDatas->GetNumElement() / m_numFrameResource;
				for (auto& currNode : currQeue->queue)
				{
					auto dirCom = currNode.first->GetComponent<COMDirLight>();
					DX12DirLightData lightData;

					if (dirCom)
					{
						lightData.color = dirCom->m_data.color;
						lightData.dir = dirCom->m_data.dir;
						lightData.power = dirCom->m_data.power;
						m_dirLightDatas->CopyData(maxLightNum * m_currFrame + m_numDirLight, &lightData);
						m_numDirLight++;
					}
				}

				for (auto& currNode : currQeue->queue)
				{
					currPso.nodeGraphicCmdFunc(m_cmdList.Get(), currNode.first, currNode.second);
					break;
				}
			}
		}
	}

	for (size_t i = PIPELINE_LIGHT_DIR + 1; i < m_lightPSOs.size(); i++)
	{
		auto& currPso = m_lightPSOs[i];

		if (currPso.pso != nullptr)
		{
			m_cmdList->SetPipelineState(currPso.pso);
			m_cmdList->SetGraphicsRootSignature(currPso.rootSig);

			currPso.baseGraphicCmdFunc(m_cmdList.Get());

			auto currQeue = currPso.renderQueue;
			if (currQeue)
			{
				for (auto& currNode : currQeue->queue)
				{
					currPso.nodeGraphicCmdFunc(m_cmdList.Get(), currNode.first, currNode.second);
				}
			}
		}
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

		m_cmdListAllocs.front()->Reset();

		ThrowIfFailed(m_d3dDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
			m_cmdListAllocs.front().Get(), nullptr, IID_PPV_ARGS(m_dataLoaderCmdList.GetAddressOf())));

		ThrowIfFailed(m_dataLoaderCmdList->Close());

		ThrowIfFailed(m_d3dDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE,
			IID_PPV_ARGS(m_fence.GetAddressOf())));
	}

	m_swapChain->CreateSwapChain(hWnd, m_commandQueue.Get(), m_backBufferFormat, windowWidth, windowHeight, 2);

	OnResize(windowWidth, windowHeight);

	BuildPso();

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

		m_d3dDevice->CreateCommittedResource(&prop, flags, &reDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(m_renderIDatMouseRead.GetAddressOf()));
	}

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
	passCons.samplerIndex = 2;
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
		CreateDeferredTextures(windowWidth, windowHeight);
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

void GraphicDeviceDX12::RenderBegin()
{
	auto cmdListAlloc = GetCurrRenderBeginCommandAllocator();

	ThrowIfFailed(cmdListAlloc->Reset());
	ThrowIfFailed(m_cmdList->Reset(cmdListAlloc, nullptr));

	m_cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_swapChain->GetDSResource(),
		D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_DEPTH_WRITE));

	auto backColor = DirectX::Colors::Gray;
	backColor.f[3] = 0.0f;
	m_swapChain->RenderBegin(m_cmdList.Get(), backColor);

	m_cmdList->RSSetViewports(1, &m_screenViewport);
	m_cmdList->RSSetScissorRects(1, &m_scissorRect);

	D3D12_CPU_DESCRIPTOR_HANDLE rtvs[] = { m_deferredRTVHeap->GetCPUDescriptorHandleForHeapStart() };
	D3D12_CPU_DESCRIPTOR_HANDLE renderIDTexture = m_deferredRTVHeap->GetCPUDescriptorHandleForHeapStart();
	renderIDTexture.ptr += m_rtvSize * DEFERRED_TEXTURE_RENDERID;

	backColor.f[0] = 0;
	backColor.f[1] = 0;
	backColor.f[2] = 0;
	backColor.f[3] = 0;
	m_cmdList->ClearRenderTargetView(renderIDTexture, backColor, 0, nullptr);

	m_cmdList->OMSetRenderTargets(DEFERRED_TEXTURE_NUM, rtvs, true, &m_swapChain->GetDSV());
}

void GraphicDeviceDX12::RenderMesh(CGHNode* node, unsigned int renderFlag)
{
	m_meshRenderQueue.queue.push_back({ node, renderFlag });
}

void GraphicDeviceDX12::RenderSkinnedMesh(CGHNode* node, unsigned int renderFlag)
{
	m_skinnedMeshRenderQueue.queue.push_back({ node, renderFlag });
}

void GraphicDeviceDX12::RenderLight(CGHNode* node, unsigned int lightFlags, size_t lightType)
{
	if (lightType == typeid(COMDirLight).hash_code())
	{
		m_dirLightRenderQueue.queue.push_back({ node, lightFlags });
	}
	else if (lightType == typeid(COMPointLight).hash_code())
	{
		m_pointLightRenderQueue.queue.push_back({ node, lightFlags });
	}
}

void GraphicDeviceDX12::RenderUI(CGHNode* node, unsigned int renderFlag)
{
	m_uiRenderQueue.queue.push_back({ node, renderFlag });
}

void GraphicDeviceDX12::RenderEnd()
{
	BaseRender();
	LightRender();

	m_meshRenderQueue.queue.clear();
	m_skinnedMeshRenderQueue.queue.clear();
	m_uiRenderQueue.queue.clear();
	m_numDirLight = 0;

	for (auto& iter : m_lightPSOs)
	{
		if (iter.renderQueue)
		{
			iter.renderQueue->queue.clear();
		}
	}

	{
		auto renderIDTexture = m_deferredResources[DEFERRED_TEXTURE_RENDERID].Get();

		D3D12_RESOURCE_BARRIER bar = {};
		bar.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		bar.Transition.pResource = renderIDTexture;
		bar.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
		bar.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
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

		
		D3D12_BOX rect = {};
		rect.left = 0;
		rect.right = 1;
		rect.top = 0;
		rect.bottom = 1;
		rect.front = 0;
		rect.back = 1;

		m_cmdList->ResourceBarrier(1, &bar);
		m_cmdList->CopyTextureRegion(&dest, 0, 0, 0, &src, &rect);

		bar.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
		bar.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
		m_cmdList->ResourceBarrier(1, &bar);
	}


	m_swapChain->RenderEnd(m_cmdList.Get());

	ThrowIfFailed(m_cmdList->Close());
	ID3D12CommandList* cmdsLists[] = { m_cmdList.Get() };
	m_commandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	m_swapChain->Present();
	m_currentFence++;
	m_fenceCounts[m_currFrame] = m_currentFence;
	ThrowIfFailed(m_commandQueue->Signal(m_fence.Get(), m_currentFence));

	unsigned int prevFrame = (m_currFrame + 2) % m_numFrameResource;
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

	BYTE* mapped = nullptr;
	D3D12_RANGE range = {};
	range.Begin = sizeof(UINT16) * prevFrame;
	range.End = range.Begin + sizeof(UINT16);

	m_renderIDatMouseRead->Map(0, &range, reinterpret_cast<void**>(&mapped));
	std::memcpy(&m_currMouseTargetRednerID, mapped, sizeof(UINT16));
	m_renderIDatMouseRead->Unmap(0, nullptr);
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
	enum
	{
		ROOT_MAINPASS_CB = 0,
		ROOT_MATERIAL_CB,
		ROOT_TEXTURE_TABLE,

		ROOT_OBJECTINFO_CB,
		ROOT_NORMAL_SRV,
		ROOT_TANGENT_SRV,
		ROOT_BITAN_SRV,
		ROOT_UV0,
		ROOT_UV1,
		ROOT_UV2,

		ROOT_WEIGHTINFO_SRV,
		ROOT_WEIGHT_SRV,
		ROOT_BONEDATA_SRV,
	};

	m_PSOs.resize(PIPELINE_WORK_NUM);

	D3D12_INPUT_ELEMENT_DESC inputElementdesc = { "POSITION" ,0,DXGI_FORMAT_R32G32B32_FLOAT,0,0,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0 };

	D3D12_ROOT_PARAMETER temp = {};
	std::vector<D3D12_ROOT_PARAMETER> rootParams;
	std::vector<D3D12_ROOT_PARAMETER> baseRoot(1);
	std::vector<D3D12_ROOT_PARAMETER> materialRoot(2);

	baseRoot[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	baseRoot[0].Descriptor.RegisterSpace = 0;
	baseRoot[0].Descriptor.ShaderRegister = 0;
	baseRoot[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	materialRoot[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	materialRoot[0].Descriptor.RegisterSpace = 0;
	materialRoot[0].Descriptor.ShaderRegister = 1;
	materialRoot[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	D3D12_DESCRIPTOR_RANGE textureTableRange[1] = {};
	textureTableRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	textureTableRange[0].RegisterSpace = 0;
	textureTableRange[0].BaseShaderRegister = 0;
	textureTableRange[0].NumDescriptors = CGHMaterial::CGHMaterialTextureNum;
	textureTableRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	materialRoot[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	materialRoot[1].DescriptorTable.NumDescriptorRanges = 1;
	materialRoot[1].DescriptorTable.pDescriptorRanges = textureTableRange;
	materialRoot[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

#pragma region PIPELINE_SKINNEDMESH
	//rootsig
	{
		rootParams.clear();
		rootParams.insert(rootParams.end(), baseRoot.begin(), baseRoot.end());
		rootParams.insert(rootParams.end(), materialRoot.begin(), materialRoot.end());

		temp.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
		temp.Constants.Num32BitValues = 1;
		temp.Constants.RegisterSpace = 1;
		temp.Constants.ShaderRegister = 0;
		temp.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		rootParams.push_back(temp);

		temp.ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
		temp.Descriptor.RegisterSpace = 1;
		temp.Descriptor.ShaderRegister = 0;
		temp.ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
		rootParams.push_back(temp);

		temp.ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
		temp.Descriptor.RegisterSpace = 1;
		temp.Descriptor.ShaderRegister = 1;
		temp.ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
		rootParams.push_back(temp);

		temp.ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
		temp.Descriptor.RegisterSpace = 1;
		temp.Descriptor.ShaderRegister = 2;
		temp.ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
		rootParams.push_back(temp);

		temp.ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
		temp.Descriptor.RegisterSpace = 1;
		temp.Descriptor.ShaderRegister = 3;
		temp.ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
		rootParams.push_back(temp);

		temp.ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
		temp.Descriptor.RegisterSpace = 1;
		temp.Descriptor.ShaderRegister = 4;
		temp.ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
		rootParams.push_back(temp);

		temp.ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
		temp.Descriptor.RegisterSpace = 1;
		temp.Descriptor.ShaderRegister = 5;
		temp.ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
		rootParams.push_back(temp);

		temp.ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
		temp.Descriptor.RegisterSpace = 2;
		temp.Descriptor.ShaderRegister = 0;
		temp.ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
		rootParams.push_back(temp);

		temp.ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
		temp.Descriptor.RegisterSpace = 2;
		temp.Descriptor.ShaderRegister = 1;
		temp.ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
		rootParams.push_back(temp);

		temp.ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
		temp.Descriptor.RegisterSpace = 2;
		temp.Descriptor.ShaderRegister = 2;
		temp.ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
		rootParams.push_back(temp);

		D3D12_ROOT_SIGNATURE_DESC rootsigDesc = {};
		rootsigDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
		rootsigDesc.pStaticSamplers = s_StaticSamplers;
		rootsigDesc.NumStaticSamplers = _countof(s_StaticSamplers);
		rootsigDesc.NumParameters = rootParams.size();
		rootsigDesc.pParameters = rootParams.data();

		DX12PipelineMG::instance.CreateRootSignature("PIPELINE_SKINNEDMESH", &rootsigDesc);

		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
		psoDesc.NodeMask = 0;

		psoDesc.pRootSignature = DX12PipelineMG::instance.GetRootSignature("PIPELINE_SKINNEDMESH");

		psoDesc.VS = DX12PipelineMG::instance.CreateShader(DX12_SHADER_VERTEX, "PIPELINE_SKINNEDMESH_VS", L"Shader/skinnedMeshShader.hlsl", "VS");
		psoDesc.PS = DX12PipelineMG::instance.CreateShader(DX12_SHADER_PIXEL, "PIPELINE_SKINNEDMESH_PS", L"Shader/skinnedMeshShader.hlsl", "PS");

		//IA Set
		psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		psoDesc.InputLayout.NumElements = 1;
		psoDesc.InputLayout.pInputElementDescs = &inputElementdesc;

		psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
		psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
		psoDesc.SampleMask = UINT_MAX;

		psoDesc.BlendState.RenderTarget[DEFERRED_TEXTURE_RENDERID].BlendEnable = false;
		psoDesc.BlendState.RenderTarget[DEFERRED_TEXTURE_RENDERID].LogicOpEnable = false;

		//OM Set
		psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
		psoDesc.NumRenderTargets = DEFERRED_TEXTURE_NUM;

		for (int i = 0; i < DEFERRED_TEXTURE_NUM; i++)
		{
			psoDesc.RTVFormats[i] = m_deferredFormat[i];
		}

		psoDesc.SampleDesc.Count = 1;
		psoDesc.SampleDesc.Quality = 0;

		DX12PipelineMG::instance.CreateGraphicPipeline("PIPELINE_SKINNEDMESH", &psoDesc);

		m_PSOs[PIPELINE_SKINNEDMESH].pso = DX12PipelineMG::instance.GetGraphicPipeline("PIPELINE_SKINNEDMESH");
		m_PSOs[PIPELINE_SKINNEDMESH].rootSig = DX12PipelineMG::instance.GetRootSignature("PIPELINE_SKINNEDMESH");

		m_PSOs[PIPELINE_SKINNEDMESH].baseGraphicCmdFunc = [this](ID3D12GraphicsCommandList* cmd)
		{
			cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			cmd->SetGraphicsRootConstantBufferView(ROOT_MAINPASS_CB, GetCurrMainPassCBV());
		};

		m_PSOs[PIPELINE_SKINNEDMESH].nodeGraphicCmdFunc = [this](ID3D12GraphicsCommandList* cmd, CGHNode* node, unsigned int renderFlag)
		{
			COMMaterial* matCom = node->GetComponent<COMMaterial>();
			COMSkinnedMesh* meshCom = node->GetComponent<COMSkinnedMesh>();
			COMDX12SkinnedMeshRenderer* renderer = node->GetComponent<COMDX12SkinnedMeshRenderer>();

			D3D12_GPU_VIRTUAL_ADDRESS matCB = matCom->GetMaterialDataGPU(m_currFrame);
			const CGHMesh* currMesh = meshCom->GetMeshData();
			auto textureDescHeap = matCom->GetTextureHeap();
			auto descHeapHandle = textureDescHeap->GetGPUDescriptorHandleForHeapStart();

			ID3D12DescriptorHeap* heaps[] = { textureDescHeap };
			cmd->SetDescriptorHeaps(_countof(heaps), heaps);

			cmd->SetGraphicsRootConstantBufferView(ROOT_MATERIAL_CB, matCB);
			cmd->SetGraphicsRootDescriptorTable(ROOT_TEXTURE_TABLE, descHeapHandle);

			D3D12_VERTEX_BUFFER_VIEW vbView = {};
			vbView.BufferLocation = currMesh->meshData[MESHDATA_POSITION]->GetGPUVirtualAddress();
			vbView.SizeInBytes = sizeof(aiVector3D) * currMesh->numData[MESHDATA_POSITION];
			vbView.StrideInBytes = sizeof(aiVector3D);

			D3D12_INDEX_BUFFER_VIEW ibView = {};
			ibView.BufferLocation = currMesh->meshData[MESHDATA_INDEX]->GetGPUVirtualAddress();
			ibView.Format = DXGI_FORMAT_R32_UINT;
			ibView.SizeInBytes = sizeof(unsigned int) * currMesh->numData[MESHDATA_INDEX];

			cmd->IASetVertexBuffers(0, 1, &vbView);
			cmd->IASetIndexBuffer(&ibView);

			unsigned int id = renderer->GetRenderID();
			cmd->SetGraphicsRoot32BitConstant(ROOT_OBJECTINFO_CB, id, 0);
			cmd->SetGraphicsRootShaderResourceView(ROOT_NORMAL_SRV, currMesh->meshData[MESHDATA_NORMAL]->GetGPUVirtualAddress());
			cmd->SetGraphicsRootShaderResourceView(ROOT_TANGENT_SRV, currMesh->meshData[MESHDATA_TAN]->GetGPUVirtualAddress());
			cmd->SetGraphicsRootShaderResourceView(ROOT_BITAN_SRV, currMesh->meshData[MESHDATA_BITAN]->GetGPUVirtualAddress());
			cmd->SetGraphicsRootShaderResourceView(ROOT_UV0, currMesh->meshDataUVs[0]->GetGPUVirtualAddress());

			if (currMesh->meshDataUVs.size() > 1)
			{
				cmd->SetGraphicsRootShaderResourceView(ROOT_UV1, currMesh->meshDataUVs[1]->GetGPUVirtualAddress());
			}
			else
			{
				cmd->SetGraphicsRootShaderResourceView(ROOT_UV1, currMesh->meshDataUVs[0]->GetGPUVirtualAddress());
			}

			if (currMesh->meshDataUVs.size() > 2)
			{
				cmd->SetGraphicsRootShaderResourceView(ROOT_UV2, currMesh->meshDataUVs[2]->GetGPUVirtualAddress());
			}
			else
			{
				cmd->SetGraphicsRootShaderResourceView(ROOT_UV2, currMesh->meshDataUVs[0]->GetGPUVirtualAddress());
			}

			cmd->SetGraphicsRootShaderResourceView(ROOT_WEIGHTINFO_SRV, currMesh->boneWeightInfos->GetGPUVirtualAddress());
			cmd->SetGraphicsRootShaderResourceView(ROOT_WEIGHT_SRV, currMesh->boneWeights->GetGPUVirtualAddress());

			cmd->SetGraphicsRootShaderResourceView(ROOT_BONEDATA_SRV, meshCom->GetBoneData(GetCurrFrameIndex()));

			cmd->DrawIndexedInstanced(currMesh->numData[MESHDATA_INDEX], 1, 0, 0, 0);
		};
	}
#pragma endregion

#pragma region PIPELINE_DEFERRED_LIGHT
	m_lightPSOs.resize(PIPELINE_LIGHT_NUM);
	{
		rootParams.clear();
		rootParams.insert(rootParams.end(), baseRoot.begin(), baseRoot.end());

		m_dirLightDatas = std::make_unique<DX12UploadBuffer<DX12DirLightData>>(m_d3dDevice.Get(), 32 * m_numFrameResource, false);

		CD3DX12_DESCRIPTOR_RANGE gbufferTexTableRange;
		gbufferTexTableRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 5, 0);
		D3D12_ROOT_PARAMETER lightBaseTable = {};
		lightBaseTable.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		lightBaseTable.DescriptorTable.NumDescriptorRanges = 1;
		lightBaseTable.DescriptorTable.pDescriptorRanges = &gbufferTexTableRange;
		rootParams.push_back(lightBaseTable);

		temp.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
		temp.Constants.RegisterSpace = 0;
		temp.Constants.ShaderRegister = 1;
		temp.Constants.Num32BitValues = 1;
		temp.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
		rootParams.push_back(temp);

		temp.ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
		temp.Descriptor.RegisterSpace = 0;
		temp.Descriptor.ShaderRegister = 5;
		temp.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
		rootParams.push_back(temp);

		D3D12_ROOT_SIGNATURE_DESC rootsigDesc = {};
		rootsigDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
		rootsigDesc.pStaticSamplers = s_StaticSamplers;
		rootsigDesc.NumStaticSamplers = _countof(s_StaticSamplers);
		rootsigDesc.NumParameters = rootParams.size();
		rootsigDesc.pParameters = rootParams.data();

		DX12PipelineMG::instance.CreateRootSignature("DEFERRED_LIGHT_DIR", &rootsigDesc);

		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
		psoDesc.NodeMask = 0;
		psoDesc.pRootSignature = DX12PipelineMG::instance.GetRootSignature("DEFERRED_LIGHT_DIR");

		psoDesc.VS = DX12PipelineMG::instance.CreateShader(DX12_SHADER_VERTEX, "DEFERRED_LIGHT_DIR_VS", L"Shader/DeferredLightDir.hlsl", "VS");
		psoDesc.GS = DX12PipelineMG::instance.CreateShader(DX12_SHADER_GEOMETRY, "DEFERRED_LIGHT_DIR_GS", L"Shader/DeferredLightDir.hlsl", "GS");
		psoDesc.PS = DX12PipelineMG::instance.CreateShader(DX12_SHADER_PIXEL, "DEFERRED_LIGHT_DIR_PS", L"Shader/DeferredLightDir.hlsl", "PS");

		//IA Set
		psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
		psoDesc.InputLayout.NumElements = 1;
		psoDesc.InputLayout.pInputElementDescs = &inputElementdesc;

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
		psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_FRONT;
		psoDesc.RasterizerState.DepthClipEnable = false;

		//OM Set
		psoDesc.DSVFormat = DXGI_FORMAT_UNKNOWN;
		psoDesc.NumRenderTargets = 1;
		psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
		psoDesc.SampleDesc.Count = 1;
		psoDesc.SampleDesc.Quality = 0;

		DX12PipelineMG::instance.CreateGraphicPipeline("DEFERRED_LIGHT_DIR", &psoDesc);

		m_lightPSOs[PIPELINE_LIGHT_DIR].pso = DX12PipelineMG::instance.GetGraphicPipeline("DEFERRED_LIGHT_DIR");
		m_lightPSOs[PIPELINE_LIGHT_DIR].rootSig = DX12PipelineMG::instance.GetRootSignature("DEFERRED_LIGHT_DIR");
		m_lightPSOs[PIPELINE_LIGHT_DIR].renderQueue = &m_dirLightRenderQueue;

		m_lightPSOs[PIPELINE_LIGHT_DIR].baseGraphicCmdFunc = [this](ID3D12GraphicsCommandList* cmd)
		{
			cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_POINTLIST);
			cmd->SetGraphicsRootConstantBufferView(ROOT_MAINPASS_CB, GetCurrMainPassCBV());

			ID3D12DescriptorHeap* heaps[] = { m_deferredBaseSRVHeap.Get() };
			cmd->SetDescriptorHeaps(1, heaps);
			cmd->SetGraphicsRootDescriptorTable(1, m_deferredBaseSRVHeap->GetGPUDescriptorHandleForHeapStart());

			cmd->IASetVertexBuffers(0, 0, nullptr);
		};

		m_lightPSOs[PIPELINE_LIGHT_DIR].nodeGraphicCmdFunc = [this](ID3D12GraphicsCommandList* cmd, CGHNode* node, unsigned int renderFlag)
		{
			auto gpuStart = m_dirLightDatas->Resource()->GetGPUVirtualAddress();
			gpuStart += (m_dirLightDatas->GetBufferSize() / m_numFrameResource) * m_currFrame;

			cmd->SetGraphicsRoot32BitConstant(2, m_numDirLight, 0);
			cmd->SetGraphicsRootShaderResourceView(3, gpuStart);
			cmd->DrawInstanced(1, 1, 0, 0);
		};

		//////////////////////////////////////////////////////////point light
		rootParams.clear();
		rootParams.insert(rootParams.end(), baseRoot.begin(), baseRoot.end());
		rootParams.push_back(lightBaseTable);

		temp.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		temp.Descriptor.RegisterSpace = 0;
		temp.Descriptor.ShaderRegister = 1;
		temp.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		rootParams.push_back(temp);

		rootsigDesc.NumParameters = rootParams.size();
		rootsigDesc.pParameters = rootParams.data();

		psoDesc.pRootSignature = DX12PipelineMG::instance.CreateRootSignature("DEFERRED_LIGHT_POINT", &rootsigDesc);

		psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH;
		psoDesc.GS = {};
		psoDesc.VS = DX12PipelineMG::instance.CreateShader(DX12_SHADER_VERTEX, "DEFERRED_LIGHT_POINT_VS", L"Shader/DeferredLightPoint.hlsl", "VS");
		psoDesc.HS = DX12PipelineMG::instance.CreateShader(DX12_SHADER_HULL, "DEFERRED_LIGHT_POINT_HS", L"Shader/DeferredLightPoint.hlsl", "HS");
		psoDesc.DS = DX12PipelineMG::instance.CreateShader(DX12_SHADER_DOMAIN, "DEFERRED_LIGHT_POINT_DS", L"Shader/DeferredLightPoint.hlsl", "DS");
		psoDesc.PS = DX12PipelineMG::instance.CreateShader(DX12_SHADER_PIXEL, "DEFERRED_LIGHT_POINT_PS", L"Shader/DeferredLightPoint.hlsl", "PS");

		DX12PipelineMG::instance.CreateGraphicPipeline("DEFERRED_LIGHT_POINT", &psoDesc);

		m_lightPSOs[PIPELINE_LIGHT_POINT].pso = DX12PipelineMG::instance.GetGraphicPipeline("DEFERRED_LIGHT_POINT");
		m_lightPSOs[PIPELINE_LIGHT_POINT].rootSig = DX12PipelineMG::instance.GetRootSignature("DEFERRED_LIGHT_POINT");
		m_lightPSOs[PIPELINE_LIGHT_POINT].renderQueue = &m_pointLightRenderQueue;

		m_lightPSOs[PIPELINE_LIGHT_POINT].baseGraphicCmdFunc = [this](ID3D12GraphicsCommandList* cmd)
		{
			cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_1_CONTROL_POINT_PATCHLIST);
			cmd->SetGraphicsRootConstantBufferView(ROOT_MAINPASS_CB, GetCurrMainPassCBV());

			ID3D12DescriptorHeap* heaps[] = { m_deferredBaseSRVHeap.Get() };
			cmd->SetDescriptorHeaps(1, heaps);
			cmd->SetGraphicsRootDescriptorTable(1, m_deferredBaseSRVHeap->GetGPUDescriptorHandleForHeapStart());

			cmd->IASetVertexBuffers(0, 0, nullptr);
		};

		m_lightPSOs[PIPELINE_LIGHT_POINT].nodeGraphicCmdFunc = [this](ID3D12GraphicsCommandList* cmd, CGHNode* node, unsigned int renderFlag)
		{
			COMPointLight* pointLight = node->GetComponent<COMPointLight>();

			cmd->SetGraphicsRootConstantBufferView(2, pointLight->GetLightDataGPU(m_currFrame));
			cmd->DrawInstanced(1, 1, 0, 0);
		};
	}

#pragma endregion



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
	rDesc.MipLevels = 0;
	rDesc.SampleDesc.Count = 1;
	rDesc.SampleDesc.Quality = 0;
	rDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	rDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

	for (unsigned int i = 0; i < DEFERRED_TEXTURE_NUM; i++)
	{
		D3D12_CLEAR_VALUE clearValue = {};
		clearValue.Format = m_deferredFormat[i];
		clearValue.Color[0] = 0;
		clearValue.Color[1] = 0;
		clearValue.Color[2] = 0;
		clearValue.Color[3] = 0;
		m_deferredResources[i] = nullptr;
		rDesc.Format = m_deferredFormat[i];
		ThrowIfFailed(m_d3dDevice->CreateCommittedResource(&prop, D3D12_HEAP_FLAG_NONE, &rDesc,
			D3D12_RESOURCE_STATE_RENDER_TARGET, &clearValue, IID_PPV_ARGS(m_deferredResources[i].GetAddressOf())));
	}

	if (m_deferredRTVHeap == nullptr)
	{
		m_rtvSize = m_d3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
		D3D12_DESCRIPTOR_HEAP_DESC heapdesc = {};
		heapdesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE::D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		heapdesc.NodeMask = 0;
		heapdesc.NumDescriptors = DEFERRED_TEXTURE_NUM;
		heapdesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		ThrowIfFailed(m_d3dDevice->CreateDescriptorHeap(&heapdesc, IID_PPV_ARGS(m_deferredRTVHeap.GetAddressOf())));

		heapdesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		heapdesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		ThrowIfFailed(m_d3dDevice->CreateDescriptorHeap(&heapdesc, IID_PPV_ARGS(m_deferredBaseSRVHeap.GetAddressOf())));
	}

	{
		auto heapCPU = m_deferredRTVHeap->GetCPUDescriptorHandleForHeapStart();
		D3D12_RENDER_TARGET_VIEW_DESC viewDesc = {};
		viewDesc.Format = DXGI_FORMAT_R11G11B10_FLOAT;
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
		auto heapCPU = m_deferredBaseSRVHeap->GetCPUDescriptorHandleForHeapStart();
		D3D12_SHADER_RESOURCE_VIEW_DESC viewDesc = {};
		viewDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		viewDesc.ViewDimension = D3D12_SRV_DIMENSION::D3D12_SRV_DIMENSION_TEXTURE2D;
		viewDesc.Texture2D.MipLevels = -1;
		viewDesc.Texture2D.MostDetailedMip = 0;
		viewDesc.Texture2D.ResourceMinLODClamp = 0;
		viewDesc.Texture2D.PlaneSlice = 0;
		viewDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

		for (int i = 0; i < DEFERRED_TEXTURE_RENDERID; i++)
		{
			viewDesc.Format = m_deferredFormat[i];
			m_d3dDevice->CreateShaderResourceView(m_deferredResources[i].Get(), &viewDesc, heapCPU);
			heapCPU.ptr += m_rtvSize;
		}

		viewDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
		m_d3dDevice->CreateShaderResourceView(m_swapChain->GetDSResource(), &viewDesc, heapCPU);
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

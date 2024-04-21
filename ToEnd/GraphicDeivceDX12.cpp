#include "GraphicDeivceDX12.h"
#include <algorithm>
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
#include "InputManager.h"
#include "Dx12FontManager.h"

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

GraphicDeviceDX12::GraphicDeviceDX12()
{

}

GraphicDeviceDX12::~GraphicDeviceDX12()
{
	if (m_uiInfoDatas != nullptr)
	{
		m_uiInfoDatas->Unmap(0, nullptr);
	}

	if (m_charInfos != nullptr)
	{
		m_charInfos->Unmap(0, nullptr);
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

		m_skinnedMeshRenderQueue.queue.clear();
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

				currPso.baseGraphicCmdFunc(m_cmdList.Get());
				currQeue->queue.clear();
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

				currQeue->queue.clear();
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

	{
		D3D12_HEAP_PROPERTIES prop = {};
		D3D12_RESOURCE_DESC reDesc = {};
		D3D12_HEAP_FLAGS flags = D3D12_HEAP_FLAG_NONE;

		prop.Type = D3D12_HEAP_TYPE_UPLOAD;

		reDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		reDesc.DepthOrArraySize = 1;
		reDesc.Height = 1;
		reDesc.Width = sizeof(UIInfo) * UIInfo::maxNumUI * m_numFrameResource;
		reDesc.Format = DXGI_FORMAT_UNKNOWN;
		reDesc.MipLevels = 1;
		reDesc.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
		reDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		reDesc.SampleDesc.Count = 1;

		ThrowIfFailed(m_d3dDevice->CreateCommittedResource(&prop, flags, &reDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(m_uiInfoDatas.GetAddressOf())));

		UIInfo* mapped = nullptr;
		ThrowIfFailed(m_uiInfoDatas->Map(0, nullptr, reinterpret_cast<void**>(&mapped)));

		for (int i = 0; i < m_numFrameResource; i++)
		{
			m_uiInfoMapped.push_back(mapped);
			mapped += UIInfo::maxNumUI;
		}

		D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
		heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		heapDesc.NumDescriptors = 1;
		heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

		ThrowIfFailed(m_d3dDevice->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(m_uiSpriteSRVHeap.GetAddressOf())));

		if (m_uiSpriteTexture != nullptr)
		{
			auto textureDesc = m_uiSpriteTexture->GetDesc();

			D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION::D3D12_SRV_DIMENSION_TEXTURE2D;
			srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			srvDesc.Format = textureDesc.Format;
			srvDesc.Texture2D.MipLevels = 1;
			srvDesc.Texture2D.MostDetailedMip = 0;
			srvDesc.Texture2D.ResourceMinLODClamp = 0;
			srvDesc.Texture2D.PlaneSlice = 0;

			m_d3dDevice->CreateShaderResourceView(m_uiSpriteTexture.Get(), &srvDesc, m_uiSpriteSRVHeap->GetCPUDescriptorHandleForHeapStart());
		}
	}

	{
		D3D12_HEAP_PROPERTIES prop = {};
		D3D12_RESOURCE_DESC desc = {};

		prop.Type = D3D12_HEAP_TYPE_UPLOAD;
		desc.DepthOrArraySize = 1;
		desc.Height = 1;
		desc.MipLevels = 1;
		desc.SampleDesc.Count = 1;
		desc.SampleDesc.Quality = 0;
		desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		desc.Width = sizeof(CGH::CharInfo) * m_maxNumChar * m_numFrameResource;

		ThrowIfFailed(m_d3dDevice->CreateCommittedResource(&prop, D3D12_HEAP_FLAG_NONE, &desc,
			D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(m_charInfos.GetAddressOf())));

		CGH::CharInfo* mapped = nullptr;
		ThrowIfFailed(m_charInfos->Map(0, nullptr, reinterpret_cast<void**>(&mapped)));

		m_charInfoMapped.resize(m_numFrameResource);

		for (auto& iter : m_charInfoMapped)
		{
			iter = mapped;
			mapped += m_maxNumChar;
		}
	}

	{
		D3D12_HEAP_PROPERTIES prop = {};
		D3D12_RESOURCE_DESC desc = {};

		prop.Type = D3D12_HEAP_TYPE_DEFAULT;

		desc.DepthOrArraySize = 1;
		desc.Height = 1;
		desc.MipLevels = 1;
		desc.SampleDesc.Count = 1;
		desc.SampleDesc.Quality = 0;
		desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
		desc.Width = sizeof(DirectX::XMFLOAT3) * UINT16_MAX;

		ThrowIfFailed(m_d3dDevice->CreateCommittedResource(&prop, D3D12_HEAP_FLAG_NONE, &desc,
			D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(m_resultVertexPosUABuffer.GetAddressOf())));

		m_cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_resultVertexPosUABuffer.Get(),
			D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_SOURCE));
	}

	ThrowIfFailed(m_cmdList->Close());

	ID3D12CommandList* cmdLists[] = { m_cmdList.Get() };
	m_commandQueue->ExecuteCommandLists(_countof(cmdLists), cmdLists);

	FlushCommandQueue();

	ThrowIfFailed(m_cmdListAllocs.front()->Reset());
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
	auto mouseState = InputManager::GetMouse().GetLastState();
	passCons.mousePos.x = mouseState.x;
	passCons.mousePos.y = mouseState.y;

	m_passCBs[m_currFrame]->CopyData(0, &passCons);

	XMVECTOR rayOrigin = XMVectorZero();
	XMVECTOR ray = XMLoadFloat3(&camera->GetViewRay(m_projMat, static_cast<unsigned int>(m_screenViewport.Width), static_cast<unsigned int>(m_screenViewport.Height)));

	rayOrigin = XMVector3Transform(rayOrigin, xmInvView);
	ray = (xmInvView.r[0] * ray.m128_f32[0]) + (xmInvView.r[1] * ray.m128_f32[1]) + (xmInvView.r[2] * ray.m128_f32[2]);

	XMStoreFloat3(&m_ray, ray);
	XMStoreFloat3(&m_rayOrigin, rayOrigin);
}

void GraphicDeviceDX12::SetFontRenderPsoWorkSet(const PipeLineWorkSet& workset)
{
	m_fontPSO = workset;
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

	auto backColor = DirectX::Colors::Gray;
	backColor.f[3] = 0.0f;
	m_swapChain->RenderBegin(m_cmdList.Get(), backColor);

	m_cmdList->RSSetViewports(1, &m_screenViewport);
	m_cmdList->RSSetScissorRects(1, &m_scissorRect);

	D3D12_CPU_DESCRIPTOR_HANDLE rtvs[] = { m_deferredRTVHeap->GetCPUDescriptorHandleForHeapStart() };
	D3D12_CPU_DESCRIPTOR_HANDLE renderIDTexture = m_deferredRTVHeap->GetCPUDescriptorHandleForHeapStart();
	D3D12_CPU_DESCRIPTOR_HANDLE renderDiffuseTexture = m_deferredRTVHeap->GetCPUDescriptorHandleForHeapStart();
	renderIDTexture.ptr += m_rtvSize * DEFERRED_TEXTURE_RENDERID;
	renderDiffuseTexture.ptr += m_rtvSize * DEFERRED_TEXTURE_DIFFUSE;

	backColor.f[0] = 0;
	backColor.f[1] = 0;
	backColor.f[2] = 0;
	backColor.f[3] = 0;
	m_cmdList->ClearRenderTargetView(renderIDTexture, backColor, 0, nullptr);
	m_cmdList->ClearRenderTargetView(renderDiffuseTexture, backColor, 0, nullptr);

	m_cmdList->OMSetRenderTargets(DEFERRED_TEXTURE_NUM, rtvs, true, &m_swapChain->GetDSV());

	for (auto& currNode : m_skinnedMeshRenderQueue.queue)
	{
		if (!(currNode.second & CGHRenderer::RENDER_FLAG_NON_TARGET_SHADOW))
		{
			auto meshCom = currNode.first->GetComponent<COMSkinnedMesh>();
			if (meshCom != nullptr)
			{
				auto iter = m_resultVertexPosBuffers.find(meshCom);

				if (iter == m_resultVertexPosBuffers.end())
				{
					auto& currBuffer = m_resultVertexPosBuffers[meshCom];

					D3D12_RESOURCE_DESC desc = meshCom->GetMeshData()->meshData[MESHDATA_POSITION]->GetDesc();
					D3D12_HEAP_PROPERTIES prop = {};
					prop.Type = D3D12_HEAP_TYPE_DEFAULT;

					ThrowIfFailed(m_d3dDevice->CreateCommittedResource(&prop, D3D12_HEAP_FLAG_NONE, &desc,
						D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(currBuffer.GetAddressOf())));

					m_cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(currBuffer.Get(),
						D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST));

					meshCom->AddDeleteEvent([this](Component* comp)
						{
							m_resultVertexPosBuffers.erase(comp);
						});
				}
				else
				{
					m_cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(iter->second.Get(),
						D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, D3D12_RESOURCE_STATE_COPY_DEST));
				}
			}

		}
	}

	for (auto& currNode : m_dirLightRenderQueue.queue)
	{
		if (!(currNode.second & CGHLightComponent::LIGHT_FLAG_SHADOW))
		{
			auto comLight = currNode.first->GetComponent<COMDirLight>();

			if (comLight == nullptr)
			{
				auto iter = m_dirLightShadowMaps.find(comLight);

				if (iter == m_dirLightShadowMaps.end())
				{
					auto& currShadowMap = m_dirLightShadowMaps[comLight];

					D3D12_RESOURCE_DESC desc = m_swapChain->GetDSResource()->GetDesc();
					D3D12_HEAP_PROPERTIES prop = {};
					prop.Type = D3D12_HEAP_TYPE_DEFAULT;

					D3D12_CLEAR_VALUE clearValue = {};
					clearValue.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
					clearValue.DepthStencil.Depth = 1.0f;
					clearValue.DepthStencil.Stencil = 0;
					ThrowIfFailed(m_d3dDevice->CreateCommittedResource(&prop, D3D12_HEAP_FLAG_NONE, &desc,
						D3D12_RESOURCE_STATE_DEPTH_WRITE, &clearValue, IID_PPV_ARGS(currShadowMap.resource.GetAddressOf())));

					D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
					heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
					heapDesc.NumDescriptors = 1;
					ThrowIfFailed(m_d3dDevice->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(currShadowMap.texViewHeap.GetAddressOf())));

					D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
					dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
					dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
					dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
					dsvDesc.Texture2D.MipSlice = 0;
					m_d3dDevice->CreateDepthStencilView(currShadowMap.resource.Get(), &dsvDesc, 
						currShadowMap.texViewHeap->GetCPUDescriptorHandleForHeapStart());

					comLight->AddDeleteEvent([this](Component* comp)
						{
							m_dirLightShadowMaps.erase(comp);
						});
				}
				else
				{
					m_cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(iter->second.resource.Get(),
						D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_DEPTH_WRITE));
				}
			}
		}
	}
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

void GraphicDeviceDX12::RenderUI(const DirectX::XMFLOAT3& pos, const DirectX::XMFLOAT2 size, const DirectX::XMFLOAT4 color, unsigned int renderID)
{
	UIInfo temp = {};
	temp.uiGraphicType = 0;
	temp.pos = pos;
	temp.size = size;
	temp.color = color;
	temp.renderID = renderID;

	m_reservedUIInfos.emplace_back(temp);
}

void GraphicDeviceDX12::RenderUI(const DirectX::XMFLOAT3& pos, const DirectX::XMFLOAT2 size, unsigned int spriteTextureSubIndex, float alpha, unsigned int renderID)
{
	UIInfo temp = {};
	temp.uiGraphicType = 1;
	temp.pos = pos;
	temp.size = size;
	temp.color.z = alpha;

	temp.renderID = renderID;

	m_reservedUIInfos.emplace_back(temp);
}

void GraphicDeviceDX12::RenderString(const wchar_t* str, const DirectX::XMFLOAT4& color, const DirectX::XMFLOAT3& pos, float size, float rowPitch)
{
	auto currFont = DX12FontManger::s_instance.GetCurrFont();
	const auto& glyphs = currFont->glyphs;
	const int stringLen = std::wcslen(str);
	XMFLOAT2 winSizeReciprocal;
	winSizeReciprocal.x = 1.0f / GO.WIN.WindowsizeX;
	winSizeReciprocal.y = 1.0f / GO.WIN.WindowsizeY;

	float offsetInString = 0.0f;
	int currLine = 0;
	CGH::CharInfo* currCharInfo = m_charInfoMapped[m_currFrame];
	currCharInfo += m_numRenderChar;

	for (int i = 0; i < stringLen; i++)
	{
		currCharInfo->glyphID = currFont->GetGlyphIndex(str[i]);
		auto& currGlyph = glyphs[currCharInfo->glyphID];
		XMFLOAT2 fontSize = { float(currGlyph.subrect.right - currGlyph.subrect.left) * size,float(currGlyph.subrect.bottom - currGlyph.subrect.top) * size };
		currCharInfo->depth = pos.z;
		currCharInfo->color = color;
		offsetInString += currGlyph.xOffset * size;
		currCharInfo->leftTopP = { pos.x + offsetInString, pos.y + (currGlyph.yOffset + (currFont->lineSpacing * currLine)) * size };
		currCharInfo->rightBottomP = { currCharInfo->leftTopP.x + fontSize.x, currCharInfo->leftTopP.y + fontSize.y };

		currCharInfo->leftTopP.x = currCharInfo->leftTopP.x * winSizeReciprocal.x * 2.0f - 1.0f;
		currCharInfo->leftTopP.y = 1.0f - (currCharInfo->leftTopP.y * winSizeReciprocal.y * 2.0f);
		currCharInfo->rightBottomP.x = currCharInfo->rightBottomP.x * winSizeReciprocal.x * 2.0f - 1.0f;
		currCharInfo->rightBottomP.y = 1.0f - (currCharInfo->rightBottomP.y * winSizeReciprocal.y * 2.0f);

		if (offsetInString + fontSize.x > rowPitch)
		{
			offsetInString = 0;
			currLine++;
		}
		else
		{
			offsetInString += fontSize.x + currGlyph.xAdvance * size;
		}

		currCharInfo++;
	}

	m_numRenderChar += stringLen;
}

void GraphicDeviceDX12::RenderEnd()
{
	BaseRender();
	LightRender();

	{
		m_cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_swapChain->GetDSResource(),
			D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_DEPTH_WRITE));

		D3D12_CPU_DESCRIPTOR_HANDLE rtvs[] = { m_swapChain->CurrRTV() };
		m_swapChain->ClearDS(m_cmdList.Get());
		m_cmdList->OMSetRenderTargets(1, rtvs, true, &m_swapChain->GetDSV());

		if (m_fontPSO.pso != nullptr)
		{
			m_cmdList->SetPipelineState(m_fontPSO.pso);
			m_cmdList->SetGraphicsRootSignature(m_fontPSO.rootSig);

			if (m_fontPSO.baseGraphicCmdFunc != nullptr)
			{
				m_fontPSO.baseGraphicCmdFunc(m_cmdList.Get());
			}
		}

		std::sort(m_reservedUIInfos.begin(), m_reservedUIInfos.end(), [](const UIInfo& left, const UIInfo& right)
			{
				return left.pos.z > right.pos.z;
			});

		std::memcpy(m_uiInfoMapped[m_currFrame], m_reservedUIInfos.data(), m_reservedUIInfos.size() * sizeof(UIInfo));

		if (m_uiPSO.pso != nullptr)
		{
			m_cmdList->SetPipelineState(m_uiPSO.pso);
			m_cmdList->SetGraphicsRootSignature(m_uiPSO.rootSig);

			if (m_uiPSO.baseGraphicCmdFunc != nullptr)
			{
				m_uiPSO.baseGraphicCmdFunc(m_cmdList.Get());
			}
		}

		m_swapChain->ClearDS(m_cmdList.Get());
		rtvs[0] = m_deferredRTVHeap->GetCPUDescriptorHandleForHeapStart();
		rtvs[0].ptr += m_rtvSize * DEFERRED_TEXTURE_RENDERID;
		m_cmdList->OMSetRenderTargets(1, rtvs, true, &m_swapChain->GetDSV());

		if (m_uiRenderIDPSO.pso != nullptr)
		{
			m_cmdList->SetPipelineState(m_uiRenderIDPSO.pso);
			m_cmdList->SetGraphicsRootSignature(m_uiRenderIDPSO.rootSig);

			if (m_uiRenderIDPSO.baseGraphicCmdFunc != nullptr)
			{
				m_uiRenderIDPSO.baseGraphicCmdFunc(m_cmdList.Get());
			}
		}

		m_reservedUIInfos.clear();
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

		auto mouseState = InputManager::GetMouse().GetLastState();
		D3D12_BOX rect = {};
		rect.left = mouseState.x;
		rect.right = mouseState.x + 1;
		rect.top = mouseState.y;
		rect.bottom = mouseState.y + 1;
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

		ROOT_RESULT_VERTEX_UAV,
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
		temp.Constants.Num32BitValues = 2;
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

		temp.ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV;
		temp.Descriptor.RegisterSpace = 1;
		temp.Descriptor.ShaderRegister = 0;
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

		psoDesc.BlendState.IndependentBlendEnable = true;

		psoDesc.BlendState.RenderTarget[DEFERRED_TEXTURE_DIFFUSE].BlendEnable = true;
		psoDesc.BlendState.RenderTarget[DEFERRED_TEXTURE_DIFFUSE].BlendOp = D3D12_BLEND_OP_ADD;
		psoDesc.BlendState.RenderTarget[DEFERRED_TEXTURE_DIFFUSE].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
		psoDesc.BlendState.RenderTarget[DEFERRED_TEXTURE_DIFFUSE].SrcBlend = D3D12_BLEND_SRC_ALPHA;
		psoDesc.BlendState.RenderTarget[DEFERRED_TEXTURE_DIFFUSE].BlendOpAlpha = D3D12_BLEND_OP_MAX;
		psoDesc.BlendState.RenderTarget[DEFERRED_TEXTURE_DIFFUSE].SrcBlend = D3D12_BLEND_ONE;
		psoDesc.BlendState.RenderTarget[DEFERRED_TEXTURE_DIFFUSE].DestBlendAlpha = D3D12_BLEND_ONE;

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
			bool isShadowGen = !(renderFlag & CGHRenderer::RENDER_FLAG_NON_TARGET_SHADOW);

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
			cmd->SetGraphicsRoot32BitConstant(ROOT_OBJECTINFO_CB, isShadowGen, 1);
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

			cmd->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_resultVertexPosUABuffer.Get(),
				D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));

			cmd->SetGraphicsRootUnorderedAccessView(ROOT_RESULT_VERTEX_UAV, m_resultVertexPosUABuffer->GetGPUVirtualAddress());

			cmd->DrawIndexedInstanced(currMesh->numData[MESHDATA_INDEX], 1, 0, 0, 0);

			cmd->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_resultVertexPosUABuffer.Get(),
				D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE));

			if (isShadowGen)
			{
				auto desc = currMesh->meshData[MESHDATA_POSITION]->GetDesc();
				auto currNodesBuffer = m_resultVertexPosBuffers[meshCom];
				cmd->CopyBufferRegion(currNodesBuffer.Get(), 0, m_resultVertexPosUABuffer.Get(), 0, desc.Width);

				cmd->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(currNodesBuffer.Get(),
					D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER));
			}
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
			if (m_numDirLight)
			{
				cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_POINTLIST);
				cmd->SetGraphicsRootConstantBufferView(ROOT_MAINPASS_CB, GetCurrMainPassCBV());

				ID3D12DescriptorHeap* heaps[] = { m_deferredBaseSRVHeap.Get() };
				cmd->SetDescriptorHeaps(1, heaps);
				cmd->SetGraphicsRootDescriptorTable(1, m_deferredBaseSRVHeap->GetGPUDescriptorHandleForHeapStart());

				cmd->IASetVertexBuffers(0, 0, nullptr);

				auto gpuStart = m_dirLightDatas->Resource()->GetGPUVirtualAddress();
				gpuStart += (m_dirLightDatas->GetBufferSize() / m_numFrameResource) * m_currFrame;

				cmd->SetGraphicsRoot32BitConstant(2, m_numDirLight, 0);
				cmd->SetGraphicsRootShaderResourceView(3, gpuStart);
				cmd->DrawInstanced(1, 1, 0, 0);
			}
		};

		m_lightPSOs[PIPELINE_LIGHT_DIR].nodeGraphicCmdFunc = nullptr;

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

#pragma region FONT_RENDER
	{
		rootParams.clear();
		std::string pipeLineName = "PIPELINE_FONT";
		temp.ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
		temp.Descriptor.RegisterSpace = 0;
		temp.Descriptor.ShaderRegister = 0;
		temp.ShaderVisibility = D3D12_SHADER_VISIBILITY_GEOMETRY;
		rootParams.push_back(temp);

		temp.ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
		temp.Descriptor.RegisterSpace = 0;
		temp.Descriptor.ShaderRegister = 1;
		temp.ShaderVisibility = D3D12_SHADER_VISIBILITY_GEOMETRY;
		rootParams.push_back(temp);

		D3D12_DESCRIPTOR_RANGE textureTableRange = {};
		textureTableRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		textureTableRange.RegisterSpace = 0;
		textureTableRange.BaseShaderRegister = 2;
		textureTableRange.NumDescriptors = 1;
		textureTableRange.OffsetInDescriptorsFromTableStart = 0;

		temp.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		temp.DescriptorTable.NumDescriptorRanges = 1;
		temp.DescriptorTable.pDescriptorRanges = &textureTableRange;
		temp.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
		rootParams.push_back(temp);

		D3D12_ROOT_SIGNATURE_DESC rootsigDesc = {};
		rootsigDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
		rootsigDesc.pStaticSamplers = s_StaticSamplers;
		rootsigDesc.NumStaticSamplers = _countof(s_StaticSamplers);
		rootsigDesc.NumParameters = rootParams.size();
		rootsigDesc.pParameters = rootParams.data();

		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
		psoDesc.NodeMask = 0;

		psoDesc.pRootSignature = DX12PipelineMG::instance.CreateRootSignature(pipeLineName.c_str(), &rootsigDesc);

		psoDesc.VS = DX12PipelineMG::instance.CreateShader(DX12_SHADER_VERTEX, (pipeLineName).c_str(), L"Shader/FontRender.hlsl", "VS");
		psoDesc.GS = DX12PipelineMG::instance.CreateShader(DX12_SHADER_GEOMETRY, (pipeLineName).c_str(), L"Shader/FontRender.hlsl", "GS");
		psoDesc.PS = DX12PipelineMG::instance.CreateShader(DX12_SHADER_PIXEL, (pipeLineName).c_str(), L"Shader/FontRender.hlsl", "PS");

		//IA Set
		psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
		psoDesc.InputLayout.NumElements = 0;
		psoDesc.InputLayout.pInputElementDescs = nullptr;

		psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_FRONT;
		psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
		psoDesc.SampleMask = UINT_MAX;

		psoDesc.BlendState.RenderTarget[0].BlendEnable = true;
		psoDesc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
		psoDesc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
		psoDesc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
		psoDesc.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_MAX;
		psoDesc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
		psoDesc.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ONE;

		//OM Set
		psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
		psoDesc.NumRenderTargets = 1;
		psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;

		psoDesc.SampleDesc.Count = 1;
		psoDesc.SampleDesc.Quality = 0;

		DX12PipelineMG::instance.CreateGraphicPipeline(pipeLineName.c_str(), &psoDesc);

		m_fontPSO.pso = DX12PipelineMG::instance.GetGraphicPipeline(pipeLineName.c_str());
		m_fontPSO.rootSig = DX12PipelineMG::instance.GetRootSignature(pipeLineName.c_str());

		m_fontPSO.baseGraphicCmdFunc = [this](ID3D12GraphicsCommandList* cmd)
		{
			auto currFont = DX12FontManger::s_instance.GetCurrFont();
			if (currFont && m_numRenderChar)
			{
				unsigned int numRenderChar = 0;
				unsigned int charInfoStructSize = sizeof(CGH::CharInfo);
				auto charInfoGPU = m_charInfos->GetGPUVirtualAddress();
				charInfoGPU += m_maxNumChar * sizeof(CGH::CharInfo) * m_currFrame;

				ID3D12DescriptorHeap* heaps[] = { currFont->textureHeap.Get() };
				cmd->SetDescriptorHeaps(1, heaps);

				cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_POINTLIST);
				cmd->SetGraphicsRootShaderResourceView(0, charInfoGPU);
				cmd->SetGraphicsRootShaderResourceView(1, currFont->glyphDatas->GetGPUVirtualAddress());
				cmd->SetGraphicsRootDescriptorTable(2, currFont->textureHeap->GetGPUDescriptorHandleForHeapStart());

				cmd->IASetVertexBuffers(0, 0, nullptr);
				cmd->IASetIndexBuffer(nullptr);

				cmd->DrawInstanced(m_numRenderChar, 1, 0, 0);

				m_numRenderChar = 0;
			}
		};

		m_fontPSO.nodeGraphicCmdFunc = nullptr;
	}
#pragma endregion

#pragma region UI_RENDER
	{
		std::string pipeLineName = "PIPELINE_UI";
		rootParams.clear();
		temp.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
		temp.Constants.Num32BitValues = 2;
		temp.Constants.RegisterSpace = 0;
		temp.Constants.ShaderRegister = 0;
		temp.ShaderVisibility = D3D12_SHADER_VISIBILITY_GEOMETRY;
		rootParams.push_back(temp);

		temp.ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
		temp.Descriptor.RegisterSpace = 0;
		temp.Descriptor.ShaderRegister = 0;
		temp.ShaderVisibility = D3D12_SHADER_VISIBILITY_GEOMETRY;
		rootParams.push_back(temp);

		D3D12_DESCRIPTOR_RANGE textureTableRange = {};
		textureTableRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		textureTableRange.RegisterSpace = 0;
		textureTableRange.BaseShaderRegister = 1;
		textureTableRange.NumDescriptors = 1;
		textureTableRange.OffsetInDescriptorsFromTableStart = 0;

		temp.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		temp.DescriptorTable.NumDescriptorRanges = 1;
		temp.DescriptorTable.pDescriptorRanges = &textureTableRange;
		temp.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
		rootParams.push_back(temp);

		D3D12_ROOT_SIGNATURE_DESC rootsigDesc = {};
		rootsigDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
		rootsigDesc.pStaticSamplers = s_StaticSamplers;
		rootsigDesc.NumStaticSamplers = _countof(s_StaticSamplers);
		rootsigDesc.NumParameters = rootParams.size();
		rootsigDesc.pParameters = rootParams.data();

		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
		psoDesc.NodeMask = 0;

		psoDesc.pRootSignature = DX12PipelineMG::instance.CreateRootSignature(pipeLineName.c_str(), &rootsigDesc);

		psoDesc.VS = DX12PipelineMG::instance.CreateShader(DX12_SHADER_VERTEX, (pipeLineName).c_str(), L"Shader/UIShader.hlsl", "VS");
		psoDesc.GS = DX12PipelineMG::instance.CreateShader(DX12_SHADER_GEOMETRY, (pipeLineName).c_str(), L"Shader/UIShader.hlsl", "GS");
		psoDesc.PS = DX12PipelineMG::instance.CreateShader(DX12_SHADER_PIXEL, (pipeLineName).c_str(), L"Shader/UIShader.hlsl", "PS");

		//IA Set
		psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
		psoDesc.InputLayout.NumElements = 0;
		psoDesc.InputLayout.pInputElementDescs = nullptr;

		psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_FRONT;
		psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
		psoDesc.SampleMask = UINT_MAX;

		psoDesc.BlendState.RenderTarget[0].BlendEnable = true;
		psoDesc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
		psoDesc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
		psoDesc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
		psoDesc.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_MAX;
		psoDesc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
		psoDesc.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ONE;

		//OM Set
		psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
		psoDesc.NumRenderTargets = 1;
		psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;

		psoDesc.SampleDesc.Count = 1;
		psoDesc.SampleDesc.Quality = 0;

		m_uiPSO.pso = DX12PipelineMG::instance.CreateGraphicPipeline(pipeLineName.c_str(), &psoDesc);
		m_uiPSO.rootSig = DX12PipelineMG::instance.GetRootSignature(pipeLineName.c_str());

		m_uiPSO.baseGraphicCmdFunc = [this](ID3D12GraphicsCommandList* cmd)
		{
			cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_POINTLIST);

			DirectX::XMUINT2 windowReciprocal = {};
			float windowXReciprocal = 1.0f / GO.WIN.WindowsizeX;
			float windowYReciprocal = 1.0f / GO.WIN.WindowsizeY;

			std::memcpy(&windowReciprocal.x, &windowXReciprocal, sizeof(float));
			std::memcpy(&windowReciprocal.y, &windowYReciprocal, sizeof(float));

			ID3D12DescriptorHeap* heaps[] = { m_uiSpriteSRVHeap.Get() };

			cmd->SetDescriptorHeaps(_countof(heaps), heaps);

			cmd->SetGraphicsRoot32BitConstant(0, windowReciprocal.x, 0);
			cmd->SetGraphicsRoot32BitConstant(0, windowReciprocal.y, 1);

			auto uiInfoDataGPU = m_uiInfoDatas->GetGPUVirtualAddress();
			uiInfoDataGPU += sizeof(UIInfo) * UIInfo::maxNumUI * m_currFrame;
			cmd->SetGraphicsRootShaderResourceView(1, uiInfoDataGPU);
			cmd->SetGraphicsRootDescriptorTable(2, m_uiSpriteSRVHeap->GetGPUDescriptorHandleForHeapStart());

			cmd->DrawInstanced(m_reservedUIInfos.size(), 1, 0, 0);
		};

		m_uiPSO.nodeGraphicCmdFunc = nullptr;
	}
#pragma endregion

#pragma region UIRENDERID_RENDER
	{
		std::string pipeLineName = "PIPELINE_UIRENDERID";
		rootParams.clear();
		temp.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
		temp.Constants.Num32BitValues = 2;
		temp.Constants.RegisterSpace = 0;
		temp.Constants.ShaderRegister = 0;
		temp.ShaderVisibility = D3D12_SHADER_VISIBILITY_GEOMETRY;
		rootParams.push_back(temp);

		temp.ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
		temp.Descriptor.RegisterSpace = 0;
		temp.Descriptor.ShaderRegister = 0;
		temp.ShaderVisibility = D3D12_SHADER_VISIBILITY_GEOMETRY;
		rootParams.push_back(temp);

		D3D12_ROOT_SIGNATURE_DESC rootsigDesc = {};
		rootsigDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
		rootsigDesc.pStaticSamplers = nullptr;
		rootsigDesc.NumStaticSamplers = 0;
		rootsigDesc.NumParameters = rootParams.size();
		rootsigDesc.pParameters = rootParams.data();

		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
		psoDesc.NodeMask = 0;

		psoDesc.pRootSignature = DX12PipelineMG::instance.CreateRootSignature(pipeLineName.c_str(), &rootsigDesc);

		psoDesc.VS = DX12PipelineMG::instance.CreateShader(DX12_SHADER_VERTEX, (pipeLineName).c_str(), L"Shader/UIRenderIDShader.hlsl", "VS");
		psoDesc.GS = DX12PipelineMG::instance.CreateShader(DX12_SHADER_GEOMETRY, (pipeLineName).c_str(), L"Shader/UIRenderIDShader.hlsl", "GS");
		psoDesc.PS = DX12PipelineMG::instance.CreateShader(DX12_SHADER_PIXEL, (pipeLineName).c_str(), L"Shader/UIRenderIDShader.hlsl", "PS");

		//IA Set
		psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
		psoDesc.InputLayout.NumElements = 0;
		psoDesc.InputLayout.pInputElementDescs = nullptr;

		psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_FRONT;
		psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
		psoDesc.SampleMask = UINT_MAX;

		psoDesc.BlendState.RenderTarget[0].BlendEnable = false;

		//OM Set
		psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
		psoDesc.NumRenderTargets = 1;
		psoDesc.RTVFormats[0] = DXGI_FORMAT_R16_UINT;

		psoDesc.SampleDesc.Count = 1;
		psoDesc.SampleDesc.Quality = 0;

		m_uiRenderIDPSO.pso = DX12PipelineMG::instance.CreateGraphicPipeline(pipeLineName.c_str(), &psoDesc);
		m_uiRenderIDPSO.rootSig = DX12PipelineMG::instance.GetRootSignature(pipeLineName.c_str());

		m_uiRenderIDPSO.baseGraphicCmdFunc = [this](ID3D12GraphicsCommandList* cmd)
		{
			cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_POINTLIST);

			DirectX::XMUINT2 windowReciprocal = {};
			float windowXReciprocal = 1.0f / GO.WIN.WindowsizeX;
			float windowYReciprocal = 1.0f / GO.WIN.WindowsizeY;

			std::memcpy(&windowReciprocal.x, &windowXReciprocal, sizeof(float));
			std::memcpy(&windowReciprocal.y, &windowYReciprocal, sizeof(float));

			cmd->SetGraphicsRoot32BitConstant(0, windowReciprocal.x, 0);
			cmd->SetGraphicsRoot32BitConstant(0, windowReciprocal.y, 1);

			auto uiInfoDataGPU = m_uiInfoDatas->GetGPUVirtualAddress();
			uiInfoDataGPU += sizeof(UIInfo) * UIInfo::maxNumUI * m_currFrame;
			cmd->SetGraphicsRootShaderResourceView(1, uiInfoDataGPU);

			cmd->DrawInstanced(m_reservedUIInfos.size(), 1, 0, 0);
		};

		m_uiRenderIDPSO.nodeGraphicCmdFunc = nullptr;
	}
#pragma endregion

#pragma region DIRLIGHT_SHADOWMAP_WRITE_PIPELINE
	{
		std::string pipeLineName = "DIRLIGHT_SHADOWMAP_WRITE";
		rootParams.clear();
		temp.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
		temp.Constants.RegisterSpace = 0;
		temp.Constants.ShaderRegister = 0;
		temp.Constants.Num32BitValues = 16;
		temp.ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
		rootParams.push_back(temp);

		D3D12_ROOT_SIGNATURE_DESC rootsigDesc = {};
		rootsigDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
		rootsigDesc.pStaticSamplers = nullptr;
		rootsigDesc.NumStaticSamplers = 0;
		rootsigDesc.NumParameters = rootParams.size();
		rootsigDesc.pParameters = rootParams.data();

		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
		psoDesc.NodeMask = 0;

		psoDesc.pRootSignature = DX12PipelineMG::instance.CreateRootSignature(pipeLineName.c_str(), &rootsigDesc);

		psoDesc.VS = DX12PipelineMG::instance.CreateShader(DX12_SHADER_VERTEX, (pipeLineName).c_str(), L"Shader/ShadowMapGenShader.hlsl", "VS");

		//IA Set
		psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		psoDesc.InputLayout.NumElements = 1;
		psoDesc.InputLayout.pInputElementDescs = &inputElementdesc;

		psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
		psoDesc.SampleMask = UINT_MAX;

		//OM Set
		psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
		psoDesc.NumRenderTargets = 0;

		psoDesc.SampleDesc.Count = 1;
		psoDesc.SampleDesc.Quality = 0;

		m_shadowMapWritePSO.pso = DX12PipelineMG::instance.CreateGraphicPipeline(pipeLineName.c_str(), &psoDesc);
		m_shadowMapWritePSO.rootSig = DX12PipelineMG::instance.GetRootSignature(pipeLineName.c_str());

		m_shadowMapWritePSO.baseGraphicCmdFunc = [this](ID3D12GraphicsCommandList* cmd)
		{
			cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		};

		m_shadowMapWritePSO.nodeGraphicCmdFunc = [this](ID3D12GraphicsCommandList* cmd, CGHNode* node, unsigned int lightFlags)
		{
			if (lightFlags & CGHLightComponent::LIGHT_FLAG_SHADOW)
			{
				auto* lightCom = node->GetComponent<COMDirLight>();
				if (lightCom)
				{
					auto shadowMap = m_dirLightShadowMaps[lightCom];
					auto shadowMapDSV = shadowMap.texViewHeap->GetCPUDescriptorHandleForHeapStart();
					DirectX::XMFLOAT4X4 shadowViewProj = {};
					cmd->ClearDepthStencilView(shadowMapDSV,
						D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);
					cmd->OMSetRenderTargets(0, nullptr, false, &shadowMapDSV);
					cmd->SetGraphicsRoot32BitConstants(0, 16, &shadowViewProj, 0);

					for (auto& iter : m_resultVertexPosBuffers)
					{
						auto meshCom = reinterpret_cast<COMSkinnedMesh*>(iter.first);
						auto mesh = meshCom->GetMeshData();

						D3D12_VERTEX_BUFFER_VIEW vbv = {};
						vbv.BufferLocation = iter.second->GetGPUVirtualAddress();
						vbv.StrideInBytes = sizeof(DirectX::XMFLOAT3);
						vbv.SizeInBytes = vbv.StrideInBytes * mesh->numData[MESHDATA_POSITION];

						D3D12_INDEX_BUFFER_VIEW ibv = {};
						ibv.BufferLocation = mesh->meshData[MESHDATA_INDEX]->GetGPUVirtualAddress();
						ibv.Format = DXGI_FORMAT_R32_UINT;
						ibv.SizeInBytes = sizeof(unsigned int) * mesh->numData[MESHDATA_INDEX];

						cmd->IASetVertexBuffers(0, 1, &vbv);
						cmd->IASetIndexBuffer(&ibv);
						cmd->DrawIndexedInstanced(mesh->numData[MESHDATA_INDEX], 1, 0, 0, 0);
					}

					cmd->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(shadowMap.resource.Get(),
						D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));
				}
			}
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

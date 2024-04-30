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
	for (auto& currPSOW : m_PSOWorkSets)
	{
		if (currPSOW.pso)
		{
			m_cmdList->SetPipelineState(currPSOW.pso);
			m_cmdList->SetGraphicsRootSignature(currPSOW.rootSig);

			if (currPSOW.baseGraphicCmdFunc != nullptr)
			{
				currPSOW.baseGraphicCmdFunc(m_cmdList.Get());
			}

			if (currPSOW.renderQueue)
			{
				for (auto& currNode : currPSOW.renderQueue->queue)
				{
					if (currPSOW.nodeGraphicCmdFunc != nullptr)
					{
						currPSOW.nodeGraphicCmdFunc(m_cmdList.Get(), currNode.first, currNode.second);
					}
				}
			}
		}
	}

	CD3DX12_RESOURCE_BARRIER resourceBars[DEFERRED_TEXTURE_RENDERID] = {};

	for (int i = 0; i < DEFERRED_TEXTURE_RENDERID; i++)
	{
		resourceBars[i] = CD3DX12_RESOURCE_BARRIER::Transition(m_deferredResources[i].Get(),
			D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET);
	}

	m_cmdList->ResourceBarrier(_countof(resourceBars), resourceBars);

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

m_dirLightDatas = std::make_unique<DX12UploadBuffer<DX12DirLightData>>(m_d3dDevice.Get(), m_numMaxDirLight * m_numFrameResource, false);
m_shadowPassCB = std::make_unique<DX12UploadBuffer<DirectX::XMMATRIX>>(m_d3dDevice.Get(), m_numMaxShadowMap * m_numFrameResource, true);

ThrowIfFailed(m_cmdList->Close());

ID3D12CommandList* cmdLists[] = { m_cmdList.Get() };
m_commandQueue->ExecuteCommandLists(_countof(cmdLists), cmdLists);

FlushCommandQueue();

ThrowIfFailed(m_cmdListAllocs.front()->Reset());

BuildPso();
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
	m_cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_swapChain->GetDSResource(),
		D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_GENERIC_READ));

	D3D12_CPU_DESCRIPTOR_HANDLE renderIDTexture = m_deferredRTVHeap->GetCPUDescriptorHandleForHeapStart();
	D3D12_CPU_DESCRIPTOR_HANDLE renderDiffuseTexture = m_deferredRTVHeap->GetCPUDescriptorHandleForHeapStart();
	renderIDTexture.ptr += m_rtvSize * DEFERRED_TEXTURE_RENDERID;
	renderDiffuseTexture.ptr += m_rtvSize * DEFERRED_TEXTURE_DIFFUSE;

	backColor = DirectX::Colors::Black;
	backColor.f[3] = 0;
	m_cmdList->ClearRenderTargetView(renderIDTexture, backColor, 0, nullptr);
	m_cmdList->ClearRenderTargetView(renderDiffuseTexture, backColor, 0, nullptr);

	CreateRenderResources();
}

void GraphicDeviceDX12::RenderMesh(CGHNode* node, unsigned int renderFlag)
{
	m_renderQueues[RENDERQUEUE_MESH].queue.push_back({ node, renderFlag });
}

void GraphicDeviceDX12::RenderSkinnedMesh(CGHNode* node, unsigned int renderFlag)
{
	m_renderQueues[RENDERQUEUE_SKINNEDMESH].queue.push_back({ node, renderFlag });
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
	winSizeReciprocal.x = 1.0f / GlobalOptions::GO.WIN.WindowsizeX;
	winSizeReciprocal.y = 1.0f / GlobalOptions::GO.WIN.WindowsizeY;

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

		if (mouseState.x < GlobalOptions::GO.WIN.WindowsizeX && mouseState.y < GlobalOptions::GO.WIN.WindowsizeY)
		{
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

void GraphicDeviceDX12::CreateRenderResources()
{
	for (auto& currNode : m_renderQueues[RENDERQUEUE_SKINNEDMESH].queue)
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
						D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER));

					meshCom->AddDeleteEvent([this](Component* comp)
						{
							m_resultVertexPosBuffers.erase(comp);
						});
				}
			}

		}
	}

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

					auto texSRVHeapCPU = m_SRVHeap->GetCPUDescriptorHandleForHeapStart();
					texSRVHeapCPU.ptr += (DEFERRED_TEXTURE_NUM + m_currNumShadowMap) * m_srvcbvuavSize;
					D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
					srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
					srvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
					srvDesc.Texture2D.MipLevels = 1;
					srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
					m_d3dDevice->CreateShaderResourceView(currShadowMap.resource.Get(), &srvDesc, texSRVHeapCPU);

					currShadowMap.srvHeapIndex = m_currNumShadowMap++;

					m_cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(currShadowMap.resource.Get(),
						D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_GENERIC_READ));

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
	BuildDeferredSkinnedMeshPipeLineWorkSet();
	BuildShadowMapWritePipeLineWorkSet();
	BuildDeferredLightDirPipeLineWorkSet();
	BuildFontRenderPipeLineWorkSet();
	BuildUIRenderPipeLineWorkSet();
	BuildUIRenderIDRenderPipeLineWorkSet();
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
		m_currNumShadowMap = 0;
		m_dirLightShadowMaps.clear();
		m_SRVHeap = nullptr;
		D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
		heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		heapDesc.NumDescriptors = DEFERRED_TEXTURE_NUM + m_numMaxShadowMap;

		ThrowIfFailed(m_d3dDevice->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(m_SRVHeap.GetAddressOf())));

		auto heapCPU = m_SRVHeap->GetCPUDescriptorHandleForHeapStart();
		D3D12_SHADER_RESOURCE_VIEW_DESC viewDesc = {};
		viewDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		viewDesc.ViewDimension = D3D12_SRV_DIMENSION::D3D12_SRV_DIMENSION_TEXTURE2D;
		viewDesc.Texture2D.MipLevels = 1;
		viewDesc.Texture2D.MostDetailedMip = 0;
		viewDesc.Texture2D.ResourceMinLODClamp = 0;
		viewDesc.Texture2D.PlaneSlice = 0;
		viewDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

		for (int i = 0; i < DEFERRED_TEXTURE_RENDERID; i++)
		{
			viewDesc.Format = m_deferredFormat[i];
			m_d3dDevice->CreateShaderResourceView(m_deferredResources[i].Get(), &viewDesc, heapCPU);
			heapCPU.ptr += m_srvcbvuavSize;
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

void GraphicDeviceDX12::BuildDeferredSkinnedMeshPipeLineWorkSet()
{
	enum
	{
		ROOT_MAINPASS_CB = 0,
		ROOT_MATERIAL_CB,
		ROOT_TEXTURE_TABLE,
		ROOT_OBJECTINFO_CB,
		ROOT_WEIGHT_SRV,
		ROOT_BONEDATA_SRV,
		ROOT_RESULT_VERTEX_UAV,
		ROOT_NUM
	};

	std::string pipeLineName = "DeferredSkinnedMesh";
	auto& currPSOWorkSet = m_PSOWorkSets[PSOW_DEFERRED_SKINNEDMESH];
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

		rootParams[ROOT_OBJECTINFO_CB].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
		rootParams[ROOT_OBJECTINFO_CB].Constants.Num32BitValues = 3;
		rootParams[ROOT_OBJECTINFO_CB].Constants.RegisterSpace = 1;
		rootParams[ROOT_OBJECTINFO_CB].Constants.ShaderRegister = 0;
		rootParams[ROOT_OBJECTINFO_CB].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

		rootParams[ROOT_WEIGHT_SRV].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
		rootParams[ROOT_WEIGHT_SRV].Descriptor.RegisterSpace = 1;
		rootParams[ROOT_WEIGHT_SRV].Descriptor.ShaderRegister = 0;
		rootParams[ROOT_WEIGHT_SRV].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

		rootParams[ROOT_BONEDATA_SRV].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
		rootParams[ROOT_BONEDATA_SRV].Descriptor.RegisterSpace = 1;
		rootParams[ROOT_BONEDATA_SRV].Descriptor.ShaderRegister = 1;
		rootParams[ROOT_BONEDATA_SRV].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

		rootParams[ROOT_RESULT_VERTEX_UAV].ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV;
		rootParams[ROOT_RESULT_VERTEX_UAV].Descriptor.RegisterSpace = 1;
		rootParams[ROOT_RESULT_VERTEX_UAV].Descriptor.ShaderRegister = 0;
		rootParams[ROOT_RESULT_VERTEX_UAV].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

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
		{ "POSITION" ,0,DXGI_FORMAT_R32G32B32_FLOAT,0,0,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0 },
	{ "NORMAL" ,0,DXGI_FORMAT_R32G32B32_FLOAT,1,D3D12_APPEND_ALIGNED_ELEMENT,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0 },
	{ "TANGENT" ,0,DXGI_FORMAT_R32G32B32_FLOAT,2,D3D12_APPEND_ALIGNED_ELEMENT,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0 },
	{ "BITAN" ,0,DXGI_FORMAT_R32G32B32_FLOAT,3,D3D12_APPEND_ALIGNED_ELEMENT,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0 },
	{ "UV" ,0,DXGI_FORMAT_R32G32B32_FLOAT,4,D3D12_APPEND_ALIGNED_ELEMENT,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0 },
	{ "UV" ,1,DXGI_FORMAT_R32G32B32_FLOAT,5,D3D12_APPEND_ALIGNED_ELEMENT,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0 },
	{ "UV" ,2,DXGI_FORMAT_R32G32B32_FLOAT,6,D3D12_APPEND_ALIGNED_ELEMENT,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0 },
	{ "BONEWEIGHTINFO",0,DXGI_FORMAT_R32G32_UINT,7,D3D12_APPEND_ALIGNED_ELEMENT,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0 }, };

	//IA Set
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH;
	psoDesc.InputLayout.NumElements = _countof(skinnedInputs);
	psoDesc.InputLayout.pInputElementDescs = skinnedInputs;

	psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	//psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
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
	psoDesc.DSVFormat = m_swapChain->GetDSVFormat();
	psoDesc.NumRenderTargets = DEFERRED_TEXTURE_NUM;

	for (int i = 0; i < DEFERRED_TEXTURE_NUM; i++)
	{
		psoDesc.RTVFormats[i] = m_deferredFormat[i];
	}

	psoDesc.SampleDesc.Count = 1;
	psoDesc.SampleDesc.Quality = 0;

	currPSOWorkSet.renderQueue = &m_renderQueues[RENDERQUEUE_SKINNEDMESH];
	currPSOWorkSet.pso = DX12PipelineMG::instance.CreateGraphicPipeline(pipeLineName.c_str(), &psoDesc);

	currPSOWorkSet.baseGraphicCmdFunc = [this](ID3D12GraphicsCommandList* cmd)
		{
			D3D12_CPU_DESCRIPTOR_HANDLE rtvs[] = { m_deferredRTVHeap->GetCPUDescriptorHandleForHeapStart() };
			cmd->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_swapChain->GetDSResource(),
				D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_DEPTH_WRITE));
			m_cmdList->OMSetRenderTargets(DEFERRED_TEXTURE_NUM, rtvs, true, &m_swapChain->GetDSV());

			cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_3_CONTROL_POINT_PATCHLIST);
			cmd->SetGraphicsRootConstantBufferView(ROOT_MAINPASS_CB, GetCurrMainPassCBV());
		};

	currPSOWorkSet.nodeGraphicCmdFunc = [this](ID3D12GraphicsCommandList* cmd, CGHNode* node, unsigned int renderFlag)
		{
			COMMaterial* matCom = node->GetComponent<COMMaterial>();
			COMSkinnedMesh* meshCom = node->GetComponent<COMSkinnedMesh>();
			COMDX12SkinnedMeshRenderer* renderer = node->GetComponent<COMDX12SkinnedMeshRenderer>();

			D3D12_GPU_VIRTUAL_ADDRESS matCB = matCom->GetMaterialDataGPU(m_currFrame);
			const CGHMesh* currMesh = meshCom->GetMeshData();
			auto textureDescHeap = matCom->GetTextureHeap();
			auto descHeapHandle = textureDescHeap->GetGPUDescriptorHandleForHeapStart();
			bool isShadowGen = !(renderFlag & CGHRenderer::RENDER_FLAG_NON_TARGET_SHADOW);
			unsigned int id = renderer->GetRenderID();

			ID3D12DescriptorHeap* heaps[] = { textureDescHeap };
			cmd->SetDescriptorHeaps(_countof(heaps), heaps);

			D3D12_VERTEX_BUFFER_VIEW vbView[8] = {};

			for (int i = 0; i < MESHDATA_INDEX; i++)
			{
				vbView[i].BufferLocation = currMesh->meshData[i]->GetGPUVirtualAddress();
				vbView[i].SizeInBytes = sizeof(aiVector3D) * currMesh->numData[MESHDATA_POSITION];
				vbView[i].StrideInBytes = sizeof(aiVector3D);
			}

			for (int i = 0; i < currMesh->meshDataUVs.size(); i++)
			{
				vbView[MESHDATA_INDEX + i].BufferLocation = currMesh->meshDataUVs[i]->GetGPUVirtualAddress();
				vbView[MESHDATA_INDEX + i].SizeInBytes = sizeof(aiVector3D) * currMesh->numData[MESHDATA_POSITION];
				vbView[MESHDATA_INDEX + i].StrideInBytes = sizeof(aiVector3D);
			}

			for (int i = currMesh->meshDataUVs.size(); i < 3; i++)
			{
				vbView[MESHDATA_INDEX + i].BufferLocation = currMesh->meshDataUVs[0]->GetGPUVirtualAddress();
				vbView[MESHDATA_INDEX + i].SizeInBytes = sizeof(aiVector3D) * currMesh->numData[MESHDATA_POSITION];
				vbView[MESHDATA_INDEX + i].StrideInBytes = sizeof(aiVector3D);
			}

			vbView[7].BufferLocation = currMesh->boneWeightInfos->GetGPUVirtualAddress();
			vbView[7].StrideInBytes = sizeof(unsigned int) * 2;
			vbView[7].SizeInBytes = vbView[7].StrideInBytes * currMesh->numData[MESHDATA_POSITION];

			UINT phongTessAlpha = reinterpret_cast<UINT&>(GlobalOptions::GO.GRAPHIC.phongTessAlpha);
			D3D12_INDEX_BUFFER_VIEW ibView = {};
			ibView.BufferLocation = currMesh->meshData[MESHDATA_INDEX]->GetGPUVirtualAddress();
			ibView.Format = DXGI_FORMAT_R32_UINT;
			ibView.SizeInBytes = sizeof(unsigned int) * currMesh->numData[MESHDATA_INDEX];

			cmd->IASetVertexBuffers(0, _countof(vbView), vbView);
			cmd->IASetVertexBuffers(0, 8, vbView);
			cmd->IASetIndexBuffer(&ibView);

			
			cmd->SetGraphicsRootConstantBufferView(ROOT_MATERIAL_CB, matCB);
			cmd->SetGraphicsRootDescriptorTable(ROOT_TEXTURE_TABLE, descHeapHandle);
			cmd->SetGraphicsRoot32BitConstant(ROOT_OBJECTINFO_CB, id, 0);
			cmd->SetGraphicsRoot32BitConstant(ROOT_OBJECTINFO_CB, isShadowGen, 1);
			cmd->SetGraphicsRoot32BitConstant(ROOT_OBJECTINFO_CB, phongTessAlpha, 2);
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

				cmd->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(currNodesBuffer.Get(),
					D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, D3D12_RESOURCE_STATE_COPY_DEST));

				cmd->CopyBufferRegion(currNodesBuffer.Get(), 0, m_resultVertexPosUABuffer.Get(), 0, desc.Width);
			}
		};
}

void GraphicDeviceDX12::BuildShadowMapWritePipeLineWorkSet()
{
	enum
	{
		ROOT_SHADOWPASS_CB = 0,
		ROOT_NUM
	};

	std::string pipeLineName = "dirShadowWrite";
	auto& currPSOWorkSet = m_PSOWorkSets[PSOW_SHADOWMAP_WRITE];
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};

	{
		std::vector<D3D12_ROOT_PARAMETER> rootParams(ROOT_NUM);

		rootParams[ROOT_SHADOWPASS_CB].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		rootParams[ROOT_SHADOWPASS_CB].Descriptor.RegisterSpace = 0;
		rootParams[ROOT_SHADOWPASS_CB].Descriptor.ShaderRegister = 0;
		rootParams[ROOT_SHADOWPASS_CB].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

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

	D3D12_INPUT_ELEMENT_DESC inputElementdesc = { "POSITION" ,0,DXGI_FORMAT_R32G32B32_FLOAT,0,0,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0 };

	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.InputLayout.NumElements = 1;
	psoDesc.InputLayout.pInputElementDescs = &inputElementdesc;

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

	currPSOWorkSet.renderQueue = &m_renderQueues[RENDERQUEUE_LIGHT_DIR];
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
						float radius = m_cameraDistance * 1.1f +2.0f;
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
						cmd->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(shadowMap.resource.Get(),
							D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_DEPTH_WRITE));
					}
				}
			}

			for (auto& iter : m_resultVertexPosBuffers)
			{
				auto meshCom = reinterpret_cast<COMSkinnedMesh*>(iter.first);
				auto mesh = meshCom->GetMeshData();

				cmd->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(iter.second.Get(),
					D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER));
			}
		};

	currPSOWorkSet.nodeGraphicCmdFunc = [this](ID3D12GraphicsCommandList* cmd, CGHNode* node, unsigned int lightFlags)
		{
			if (lightFlags & CGHLightComponent::LIGHT_FLAG_SHADOW)
			{
				auto* lightCom = node->GetComponent<COMDirLight>();
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
						cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

						cmd->DrawIndexedInstanced(mesh->numData[MESHDATA_INDEX], 1, 0, 0, 0);
					}
				}
			}
		};
}

void GraphicDeviceDX12::BuildDeferredLightDirPipeLineWorkSet()
{
	enum
	{
		ROOT_MAINPASS_CB = 0,
		ROOT_TEXTURE_TABLE,
		ROOT_NUMLIGHT_CONST,
		ROOT_LIGHTDATA_SRV,
		ROOT_NUM
	};

	std::string pipeLineName = "DeferredLightDir";
	auto& currPSOWorkSet = m_PSOWorkSets[PSOW_DEFERRED_LIGHT_DIR];
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

		rootParams[ROOT_NUMLIGHT_CONST].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
		rootParams[ROOT_NUMLIGHT_CONST].Constants.RegisterSpace = 0;
		rootParams[ROOT_NUMLIGHT_CONST].Constants.ShaderRegister = 1;
		rootParams[ROOT_NUMLIGHT_CONST].Constants.Num32BitValues = 1;
		rootParams[ROOT_NUMLIGHT_CONST].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

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

	currPSOWorkSet.renderQueue = nullptr;
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

					if (lights[m_numDirLight].second & CGHLightComponent::LIGHT_FLAG_SHADOW)
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

						cmd->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(shadowMap.resource.Get(),
							D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_GENERIC_READ));
					}

					m_dirLightDatas->CopyData((m_numMaxDirLight * m_currFrame) + m_numDirLight, lightData);
				}
			}

			CD3DX12_RESOURCE_BARRIER resourceBars[DEFERRED_TEXTURE_RENDERID + 1] = {};

			for (int i = 0; i < DEFERRED_TEXTURE_RENDERID; i++)
			{
				resourceBars[i] = CD3DX12_RESOURCE_BARRIER::Transition(m_deferredResources[i].Get(),
					D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ);
			}

			resourceBars[DEFERRED_TEXTURE_RENDERID] = CD3DX12_RESOURCE_BARRIER::Transition(m_swapChain->GetDSResource(),
				D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_GENERIC_READ);

			cmd->ResourceBarrier(_countof(resourceBars), resourceBars);
			auto rtv = m_swapChain->CurrRTV();
			cmd->OMSetRenderTargets(1, &rtv, false, nullptr);

			if (m_numDirLight)
			{
				cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_POINTLIST);
				cmd->SetGraphicsRootConstantBufferView(ROOT_MAINPASS_CB, GetCurrMainPassCBV());

				ID3D12DescriptorHeap* heaps[] = { m_SRVHeap.Get() };
				cmd->SetDescriptorHeaps(1, heaps);

				cmd->IASetVertexBuffers(0, 0, nullptr);

				auto ligthDataStart = m_dirLightDatas->Resource()->GetGPUVirtualAddress();
				ligthDataStart += (m_numMaxDirLight * m_currFrame) * m_dirLightDatas->GetElementByteSize();

				cmd->SetGraphicsRootDescriptorTable(ROOT_TEXTURE_TABLE, m_SRVHeap->GetGPUDescriptorHandleForHeapStart());
				cmd->SetGraphicsRoot32BitConstant(ROOT_NUMLIGHT_CONST, m_numDirLight, 0);
				cmd->SetGraphicsRootShaderResourceView(ROOT_LIGHTDATA_SRV, ligthDataStart);

				cmd->DrawInstanced(1, 1, 0, 0);
			}
		};

	currPSOWorkSet.nodeGraphicCmdFunc = nullptr;
}

void GraphicDeviceDX12::BuildFontRenderPipeLineWorkSet()
{
	enum
	{
		ROOT_CHARINFO_SRV = 0,
		ROOT_GLYPHS_SRV,
		ROOT_SPRITETEX_TABLE,
		ROOT_NUM
	};

	std::string pipeLineName = "fontRender";
	auto& currPSOWorkSet = m_PSOWorkSets[PSOW_FONT_RENDER];
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};

	{
		std::vector<D3D12_ROOT_PARAMETER> rootParams(ROOT_NUM);
		rootParams[ROOT_CHARINFO_SRV].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
		rootParams[ROOT_CHARINFO_SRV].Descriptor.RegisterSpace = 0;
		rootParams[ROOT_CHARINFO_SRV].Descriptor.ShaderRegister = 0;
		rootParams[ROOT_CHARINFO_SRV].ShaderVisibility = D3D12_SHADER_VISIBILITY_GEOMETRY;

		rootParams[ROOT_GLYPHS_SRV].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
		rootParams[ROOT_GLYPHS_SRV].Descriptor.RegisterSpace = 0;
		rootParams[ROOT_GLYPHS_SRV].Descriptor.ShaderRegister = 1;
		rootParams[ROOT_GLYPHS_SRV].ShaderVisibility = D3D12_SHADER_VISIBILITY_GEOMETRY;

		D3D12_DESCRIPTOR_RANGE textureTableRange = {};
		textureTableRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		textureTableRange.RegisterSpace = 0;
		textureTableRange.BaseShaderRegister = 2;
		textureTableRange.NumDescriptors = 1;
		textureTableRange.OffsetInDescriptorsFromTableStart = 0;

		rootParams[ROOT_SPRITETEX_TABLE].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		rootParams[ROOT_SPRITETEX_TABLE].DescriptorTable.NumDescriptorRanges = 1;
		rootParams[ROOT_SPRITETEX_TABLE].DescriptorTable.pDescriptorRanges = &textureTableRange;
		rootParams[ROOT_SPRITETEX_TABLE].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

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

	psoDesc.VS = DX12PipelineMG::instance.CreateShader(DX12_SHADER_VERTEX, (pipeLineName).c_str(), L"Shader/FontRender.hlsl", "VS");
	psoDesc.GS = DX12PipelineMG::instance.CreateShader(DX12_SHADER_GEOMETRY, (pipeLineName).c_str(), L"Shader/FontRender.hlsl", "GS");
	psoDesc.PS = DX12PipelineMG::instance.CreateShader(DX12_SHADER_PIXEL, (pipeLineName).c_str(), L"Shader/FontRender.hlsl", "PS");

	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
	psoDesc.InputLayout.NumElements = 0;
	psoDesc.InputLayout.pInputElementDescs = nullptr;

	psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
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

	psoDesc.DSVFormat = m_swapChain->GetDSVFormat();
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = m_backBufferFormat;
	psoDesc.SampleDesc.Count = 1;
	psoDesc.SampleDesc.Quality = 0;

	DX12PipelineMG::instance.CreateGraphicPipeline(pipeLineName.c_str(), &psoDesc);

	currPSOWorkSet.renderQueue = nullptr;
	currPSOWorkSet.pso = DX12PipelineMG::instance.GetGraphicPipeline(pipeLineName.c_str());

	currPSOWorkSet.baseGraphicCmdFunc = [this](ID3D12GraphicsCommandList* cmd)
		{
			auto rtv = m_swapChain->CurrRTV();
			cmd->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_swapChain->GetDSResource(),
				D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_DEPTH_WRITE));
			m_swapChain->ClearDS(cmd);
			cmd->OMSetRenderTargets(1, &rtv, false, &m_swapChain->GetDSV());

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
				cmd->SetGraphicsRootShaderResourceView(ROOT_CHARINFO_SRV, charInfoGPU);
				cmd->SetGraphicsRootShaderResourceView(ROOT_GLYPHS_SRV, currFont->glyphDatas->GetGPUVirtualAddress());
				cmd->SetGraphicsRootDescriptorTable(ROOT_SPRITETEX_TABLE, currFont->textureHeap->GetGPUDescriptorHandleForHeapStart());

				cmd->IASetVertexBuffers(0, 0, nullptr);
				cmd->IASetIndexBuffer(nullptr);

				cmd->DrawInstanced(m_numRenderChar, 1, 0, 0);

				m_numRenderChar = 0;
			}
		};

	currPSOWorkSet.nodeGraphicCmdFunc = nullptr;
}

void GraphicDeviceDX12::BuildUIRenderPipeLineWorkSet()
{
	enum
	{
		ROOT_WINSIZERECIPROCAL_CONST = 0,
		ROOT_UIINFOS_SRV,
		ROOT_SPRITETEX_TABLE,
		ROOT_NUM
	};

	std::string pipeLineName = "uiRender";
	auto& currPSOWorkSet = m_PSOWorkSets[PSOW_UI_RENDER];
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};

	{
		std::vector<D3D12_ROOT_PARAMETER> rootParams(ROOT_NUM);
		rootParams[ROOT_WINSIZERECIPROCAL_CONST].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
		rootParams[ROOT_WINSIZERECIPROCAL_CONST].Constants.Num32BitValues = 2;
		rootParams[ROOT_WINSIZERECIPROCAL_CONST].Constants.RegisterSpace = 0;
		rootParams[ROOT_WINSIZERECIPROCAL_CONST].Constants.ShaderRegister = 0;
		rootParams[ROOT_WINSIZERECIPROCAL_CONST].ShaderVisibility = D3D12_SHADER_VISIBILITY_GEOMETRY;

		rootParams[ROOT_UIINFOS_SRV].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
		rootParams[ROOT_UIINFOS_SRV].Descriptor.RegisterSpace = 0;
		rootParams[ROOT_UIINFOS_SRV].Descriptor.ShaderRegister = 0;
		rootParams[ROOT_UIINFOS_SRV].ShaderVisibility = D3D12_SHADER_VISIBILITY_GEOMETRY;

		D3D12_DESCRIPTOR_RANGE textureTableRange = {};
		textureTableRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		textureTableRange.RegisterSpace = 0;
		textureTableRange.BaseShaderRegister = 1;
		textureTableRange.NumDescriptors = 1;
		textureTableRange.OffsetInDescriptorsFromTableStart = 0;

		rootParams[ROOT_SPRITETEX_TABLE].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		rootParams[ROOT_SPRITETEX_TABLE].DescriptorTable.NumDescriptorRanges = 1;
		rootParams[ROOT_SPRITETEX_TABLE].DescriptorTable.pDescriptorRanges = &textureTableRange;
		rootParams[ROOT_SPRITETEX_TABLE].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

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

	psoDesc.VS = DX12PipelineMG::instance.CreateShader(DX12_SHADER_VERTEX, (pipeLineName).c_str(), L"Shader/UIShader.hlsl", "VS");
	psoDesc.GS = DX12PipelineMG::instance.CreateShader(DX12_SHADER_GEOMETRY, (pipeLineName).c_str(), L"Shader/UIShader.hlsl", "GS");
	psoDesc.PS = DX12PipelineMG::instance.CreateShader(DX12_SHADER_PIXEL, (pipeLineName).c_str(), L"Shader/UIShader.hlsl", "PS");

	//IA Set
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
	psoDesc.InputLayout.NumElements = 0;
	psoDesc.InputLayout.pInputElementDescs = nullptr;

	psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
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
	psoDesc.DSVFormat = m_swapChain->GetDSVFormat();
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = m_backBufferFormat;

	psoDesc.SampleDesc.Count = 1;
	psoDesc.SampleDesc.Quality = 0;

	currPSOWorkSet.renderQueue = nullptr;
	currPSOWorkSet.pso = DX12PipelineMG::instance.CreateGraphicPipeline(pipeLineName.c_str(), &psoDesc);

	currPSOWorkSet.baseGraphicCmdFunc = [this](ID3D12GraphicsCommandList* cmd)
		{
			std::sort(m_reservedUIInfos.begin(), m_reservedUIInfos.end(), [](const UIInfo& left, const UIInfo& right)
				{
					return left.pos.z > right.pos.z;
				});

			std::memcpy(m_uiInfoMapped[m_currFrame], m_reservedUIInfos.data(), m_reservedUIInfos.size() * sizeof(UIInfo));

			cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_POINTLIST);

			DirectX::XMUINT2 windowReciprocal = {};
			float windowXReciprocal = 1.0f / GlobalOptions::GO.WIN.WindowsizeX;
			float windowYReciprocal = 1.0f / GlobalOptions::GO.WIN.WindowsizeY;

			std::memcpy(&windowReciprocal.x, &windowXReciprocal, sizeof(float));
			std::memcpy(&windowReciprocal.y, &windowYReciprocal, sizeof(float));

			ID3D12DescriptorHeap* heaps[] = { m_uiSpriteSRVHeap.Get() };

			cmd->SetDescriptorHeaps(_countof(heaps), heaps);

			auto uiInfoDataGPU = m_uiInfoDatas->GetGPUVirtualAddress();
			uiInfoDataGPU += sizeof(UIInfo) * UIInfo::maxNumUI * m_currFrame;

			cmd->SetGraphicsRoot32BitConstant(ROOT_WINSIZERECIPROCAL_CONST, windowReciprocal.x, 0);
			cmd->SetGraphicsRoot32BitConstant(ROOT_WINSIZERECIPROCAL_CONST, windowReciprocal.y, 1);
			cmd->SetGraphicsRootShaderResourceView(ROOT_UIINFOS_SRV, uiInfoDataGPU);
			cmd->SetGraphicsRootDescriptorTable(ROOT_SPRITETEX_TABLE, m_uiSpriteSRVHeap->GetGPUDescriptorHandleForHeapStart());

			cmd->DrawInstanced(m_reservedUIInfos.size(), 1, 0, 0);
		};

	currPSOWorkSet.nodeGraphicCmdFunc = nullptr;
}

void GraphicDeviceDX12::BuildUIRenderIDRenderPipeLineWorkSet()
{
	enum
	{
		ROOT_WINSIZERECIPROCAL_CONST = 0,
		ROOT_UIINFOS_SRV,
		ROOT_NUM
	};

	std::string pipeLineName = "uiRenderIDRender";
	auto& currPSOWorkSet = m_PSOWorkSets[PSOW_UI_RENDERID_RENDER];
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};

	{
		std::vector<D3D12_ROOT_PARAMETER> rootParams(ROOT_NUM);

		rootParams[ROOT_WINSIZERECIPROCAL_CONST].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
		rootParams[ROOT_WINSIZERECIPROCAL_CONST].Constants.Num32BitValues = 2;
		rootParams[ROOT_WINSIZERECIPROCAL_CONST].Constants.RegisterSpace = 0;
		rootParams[ROOT_WINSIZERECIPROCAL_CONST].Constants.ShaderRegister = 0;
		rootParams[ROOT_WINSIZERECIPROCAL_CONST].ShaderVisibility = D3D12_SHADER_VISIBILITY_GEOMETRY;

		rootParams[ROOT_UIINFOS_SRV].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
		rootParams[ROOT_UIINFOS_SRV].Descriptor.RegisterSpace = 0;
		rootParams[ROOT_UIINFOS_SRV].Descriptor.ShaderRegister = 0;
		rootParams[ROOT_UIINFOS_SRV].ShaderVisibility = D3D12_SHADER_VISIBILITY_GEOMETRY;

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

	psoDesc.VS = DX12PipelineMG::instance.CreateShader(DX12_SHADER_VERTEX, (pipeLineName).c_str(), L"Shader/UIRenderIDShader.hlsl", "VS");
	psoDesc.GS = DX12PipelineMG::instance.CreateShader(DX12_SHADER_GEOMETRY, (pipeLineName).c_str(), L"Shader/UIRenderIDShader.hlsl", "GS");
	psoDesc.PS = DX12PipelineMG::instance.CreateShader(DX12_SHADER_PIXEL, (pipeLineName).c_str(), L"Shader/UIRenderIDShader.hlsl", "PS");

	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
	psoDesc.InputLayout.NumElements = 0;
	psoDesc.InputLayout.pInputElementDescs = nullptr;

	psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	psoDesc.SampleMask = UINT_MAX;

	psoDesc.BlendState.RenderTarget[0].BlendEnable = false;

	psoDesc.DSVFormat = m_swapChain->GetDSVFormat();
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R16_UINT;
	psoDesc.SampleDesc.Count = 1;
	psoDesc.SampleDesc.Quality = 0;

	currPSOWorkSet.renderQueue = nullptr;
	currPSOWorkSet.pso = DX12PipelineMG::instance.CreateGraphicPipeline(pipeLineName.c_str(), &psoDesc);

	currPSOWorkSet.baseGraphicCmdFunc = [this](ID3D12GraphicsCommandList* cmd)
		{
			auto rtv = m_deferredRTVHeap->GetCPUDescriptorHandleForHeapStart();
			rtv.ptr += m_rtvSize * DEFERRED_TEXTURE_RENDERID;
			m_swapChain->ClearDS(cmd);
			cmd->OMSetRenderTargets(1, &rtv, false, &m_swapChain->GetDSV());

			cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_POINTLIST);

			DirectX::XMUINT2 windowReciprocal = {};
			float windowXReciprocal = 1.0f / GlobalOptions::GO.WIN.WindowsizeX;
			float windowYReciprocal = 1.0f / GlobalOptions::GO.WIN.WindowsizeY;

			std::memcpy(&windowReciprocal.x, &windowXReciprocal, sizeof(float));
			std::memcpy(&windowReciprocal.y, &windowYReciprocal, sizeof(float));

			auto uiInfoDataGPU = m_uiInfoDatas->GetGPUVirtualAddress();
			uiInfoDataGPU += sizeof(UIInfo) * UIInfo::maxNumUI * m_currFrame;

			cmd->SetGraphicsRoot32BitConstant(ROOT_WINSIZERECIPROCAL_CONST, windowReciprocal.x, 0);
			cmd->SetGraphicsRoot32BitConstant(ROOT_WINSIZERECIPROCAL_CONST, windowReciprocal.y, 1);
			cmd->SetGraphicsRootShaderResourceView(ROOT_UIINFOS_SRV, uiInfoDataGPU);

			cmd->DrawInstanced(m_reservedUIInfos.size(), 1, 0, 0);

			m_reservedUIInfos.clear();
		};

	currPSOWorkSet.nodeGraphicCmdFunc = nullptr;
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
	auto currPSOWorkSet = &m_PSOWorkSets[PSOW_SMAA_EDGE_RENDER];
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
	{
		std::vector<D3D12_ROOT_PARAMETER> rootParams(ROOT_NUM);
		rootParams[ROOT_SMAA_PASS_CONST].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
		rootParams[ROOT_SMAA_PASS_CONST].Constants.Num32BitValues = 8;
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
		rootsigDesc.pStaticSamplers = nullptr;
		rootsigDesc.NumStaticSamplers =0;
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

		psoDesc.DSVFormat = DXGI_FORMAT_UNKNOWN;
		psoDesc.NumRenderTargets = 1;
		psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8_UNORM;

		currPSOWorkSet->renderQueue = nullptr;
		currPSOWorkSet->pso = DX12PipelineMG::instance.CreateGraphicPipeline((pipeLineName + "EDGE").c_str(), &psoDesc);
		currPSOWorkSet->rootSig = DX12PipelineMG::instance.GetRootSignature(pipeLineName.c_str());
	}
	

	{
		currPSOWorkSet = &m_PSOWorkSets[PSOW_SMAA_BLEND_RENDER];

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

		psoDesc.DSVFormat = DXGI_FORMAT_UNKNOWN;
		psoDesc.NumRenderTargets = 1;
		psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;

		currPSOWorkSet->renderQueue = nullptr;
		currPSOWorkSet->pso = DX12PipelineMG::instance.CreateGraphicPipeline((pipeLineName + "BLEND").c_str(), &psoDesc);
		currPSOWorkSet->rootSig = DX12PipelineMG::instance.GetRootSignature(pipeLineName.c_str());
	}

	{
		currPSOWorkSet = &m_PSOWorkSets[PSOW_SMAA_NEIBLEND_RENDER];

		psoDesc.GS = DX12PipelineMG::instance.CreateShader(DX12_SHADER_GEOMETRY, (pipeLineName + "NEIBLEND").c_str(), L"Shader/SMAAShader.hlsl", "NeiBlendGS");
		psoDesc.PS = DX12PipelineMG::instance.CreateShader(DX12_SHADER_PIXEL, (pipeLineName + "NEIBLEND").c_str(), L"Shader/SMAAShader.hlsl", "NeiBlendPS");

		psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);

		psoDesc.DepthStencilState.DepthEnable = false;
		psoDesc.DepthStencilState.StencilEnable = false;
		psoDesc.SampleMask = UINT_MAX;

		psoDesc.BlendState.RenderTarget[0].BlendEnable = false;

		psoDesc.DSVFormat = DXGI_FORMAT_UNKNOWN;
		psoDesc.NumRenderTargets = 1;
		psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;

		currPSOWorkSet->renderQueue = nullptr;
		currPSOWorkSet->pso = DX12PipelineMG::instance.CreateGraphicPipeline((pipeLineName + "BLEND").c_str(), &psoDesc);
		currPSOWorkSet->rootSig = DX12PipelineMG::instance.GetRootSignature(pipeLineName.c_str());
	}
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
	auto& currPSOWorkSet = m_PSOWorkSets[PSOW_TEX_DEBUG];
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};

	{
		std::vector<D3D12_ROOT_PARAMETER> rootParams(ROOT_NUM);

		rootParams[ROOT_WINSIZE_CONST].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
		rootParams[ROOT_WINSIZE_CONST].Constants.Num32BitValues = 2;
		rootParams[ROOT_WINSIZE_CONST].Constants.RegisterSpace = 0;
		rootParams[ROOT_WINSIZE_CONST].Constants.ShaderRegister = 0;
		rootParams[ROOT_WINSIZE_CONST].ShaderVisibility = D3D12_SHADER_VISIBILITY_GEOMETRY;

		CD3DX12_DESCRIPTOR_RANGE shadowTexTableRange[2];
		shadowTexTableRange[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, DEFERRED_TEXTURE_NUM, 0);
		shadowTexTableRange[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, m_numMaxShadowMap, 0, 1);

		rootParams[ROOT_DEBUG_TEXTURE_TABLE].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		rootParams[ROOT_DEBUG_TEXTURE_TABLE].DescriptorTable.NumDescriptorRanges = 2;
		rootParams[ROOT_DEBUG_TEXTURE_TABLE].DescriptorTable.pDescriptorRanges = shadowTexTableRange;
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

	currPSOWorkSet.renderQueue = nullptr;
	currPSOWorkSet.pso = DX12PipelineMG::instance.CreateGraphicPipeline(pipeLineName.c_str(), &psoDesc);

	currPSOWorkSet.baseGraphicCmdFunc = [this](ID3D12GraphicsCommandList* cmd)
		{
			auto rtv = m_swapChain->CurrRTV();
			cmd->OMSetRenderTargets(1, &rtv, false, nullptr);

			cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_POINTLIST);

			ID3D12DescriptorHeap* descHeaps[] = { m_SRVHeap.Get() };
			cmd->SetDescriptorHeaps(1, descHeaps);
			cmd->SetGraphicsRoot32BitConstant(ROOT_WINSIZE_CONST, GlobalOptions::GO.WIN.WindowsizeX, 0);
			cmd->SetGraphicsRoot32BitConstant(ROOT_WINSIZE_CONST, GlobalOptions::GO.WIN.WindowsizeY, 1);

			auto srvHeap = m_SRVHeap->GetGPUDescriptorHandleForHeapStart();
			cmd->SetGraphicsRootDescriptorTable(ROOT_DEBUG_TEXTURE_TABLE, srvHeap);

			cmd->DrawInstanced(1, 1, 0, 0);
		};

	currPSOWorkSet.nodeGraphicCmdFunc = nullptr;
}

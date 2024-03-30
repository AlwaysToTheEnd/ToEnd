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

	m_cmdList->OMSetRenderTargets(1, rtvs, true, &presentDSV);

	for (auto& currPso : m_lightPSOs)
	{
		m_cmdList->SetPipelineState(currPso.pso);
		m_cmdList->SetGraphicsRootSignature(currPso.rootSig);

		currPso.baseGraphicCmdFunc(m_cmdList.Get());

		for (auto& currNode : m_dirLightRenderQueue.queue)
		{	

			currPso.nodeGraphicCmdFunc(m_cmdList.Get(), currNode.first, currNode.second);
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

	XMMATRIX proj = XMMatrixPerspectiveFovLH(fovAngleY, aspectRatio, 1.0f, 1000.0f);
	XMStoreFloat4x4(&m_projMat, proj);

	FlushCommandQueue();
	ThrowIfFailed(cmdListAlloc->Reset());
}

void GraphicDeviceDX12::LoadMeshDataFile(const char* filePath, std::vector<CGHMesh>* outMeshSet,
	std::vector<CGHMaterial>* outMaterials, std::vector<CGHNode>* outNode)
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

	D3D12_CPU_DESCRIPTOR_HANDLE rtvs[] = { m_deferredRTVHeap->GetCPUDescriptorHandleForHeapStart() };

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
	/*else if ()
	{

	}*/
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
	m_dirLightRenderQueue.queue.clear();

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
		temp.Descriptor.RegisterSpace = 1;
		temp.Descriptor.ShaderRegister = 0;
		temp.ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
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
		psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
		psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
		psoDesc.SampleMask = UINT_MAX;

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
			cmd->SetGraphicsRootConstantBufferView(ROOT_MAINPASS_CB, GetCurrMainPassCBV());
		};

		m_PSOs[PIPELINE_SKINNEDMESH].nodeGraphicCmdFunc = [this](ID3D12GraphicsCommandList* cmd, CGHNode* node, unsigned int renderFlag)
		{
			COMMaterial* matCom = node->GetComponent<COMMaterial>();
			COMSkinnedMesh* meshCom = node->GetComponent<COMSkinnedMesh>();
			COMDX12SkinnedMeshRenderer* renderer = node->GetComponent<COMDX12SkinnedMeshRenderer>();

			cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

			D3D12_GPU_VIRTUAL_ADDRESS matCB = matCom->GetMaterialDataGPU();
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

			cmd->SetGraphicsRoot32BitConstant(ROOT_OBJECTINFO_CB, renderer->GetRenderID(), 0);
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

	m_lightPSOs.resize(PIPELINELIGHT_NUM);
#pragma region PIPELINE_DEFERRED_LIGHT
	{
		std::vector<D3D12_ROOT_PARAMETER> lightBaseRoot(5);
		
		lightBaseRoot[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
		lightBaseRoot[0].Descriptor.RegisterSpace = 0;
		lightBaseRoot[0].Descriptor.ShaderRegister = 0;
		lightBaseRoot[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

		lightBaseRoot[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
		lightBaseRoot[1].Descriptor.RegisterSpace = 0;
		lightBaseRoot[1].Descriptor.ShaderRegister = 1;
		lightBaseRoot[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

		lightBaseRoot[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
		lightBaseRoot[2].Descriptor.RegisterSpace = 0;
		lightBaseRoot[2].Descriptor.ShaderRegister = 2;
		lightBaseRoot[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

		lightBaseRoot[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
		lightBaseRoot[3].Descriptor.RegisterSpace = 0;
		lightBaseRoot[3].Descriptor.ShaderRegister = 3;
		lightBaseRoot[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

		lightBaseRoot[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
		lightBaseRoot[4].Descriptor.RegisterSpace = 0;
		lightBaseRoot[4].Descriptor.ShaderRegister = 4;
		lightBaseRoot[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

		rootParams.clear();
		rootParams.insert(rootParams.end(), baseRoot.begin(), baseRoot.end());
		rootParams.insert(rootParams.end(), lightBaseRoot.begin(), lightBaseRoot.end());

		temp.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		temp.Descriptor.RegisterSpace = 1;
		temp.Descriptor.ShaderRegister = 0;
		temp.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		rootParams.push_back(temp);

		D3D12_ROOT_SIGNATURE_DESC rootsigDesc = {};
		rootsigDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
		rootsigDesc.pStaticSamplers = s_StaticSamplers;
		rootsigDesc.NumStaticSamplers = _countof(s_StaticSamplers);
		rootsigDesc.NumParameters = rootParams.size();
		rootsigDesc.pParameters = rootParams.data();

		DX12PipelineMG::instance.CreateRootSignature("PIPELINE_DEFERRED_LIGHT", &rootsigDesc);

		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
		psoDesc.NodeMask = 0;
		psoDesc.pRootSignature = DX12PipelineMG::instance.GetRootSignature("PIPELINE_DEFERRED_LIGHT");

		psoDesc.VS = DX12PipelineMG::instance.CreateShader(DX12_SHADER_VERTEX, "DEFERRED_LIGHT_DIR_VS", L"Shader/skinnedMeshShader.hlsl", "VS");
		psoDesc.GS = DX12PipelineMG::instance.CreateShader(DX12_SHADER_GEOMETRY, "DEFERRED_LIGHT_DIR_GS", L"Shader/skinnedMeshShader.hlsl", "GS");
		psoDesc.PS = DX12PipelineMG::instance.CreateShader(DX12_SHADER_PIXEL, "DEFERRED_LIGHT_DIR_PS", L"Shader/skinnedMeshShader.hlsl", "PS");

		//IA Set
		psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
		psoDesc.InputLayout.NumElements = 1;
		psoDesc.InputLayout.pInputElementDescs = &inputElementdesc;

		psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
		psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
		psoDesc.SampleMask = UINT_MAX;

		//OM Set
		psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
		psoDesc.NumRenderTargets = 1;
		psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
		psoDesc.SampleDesc.Count = 1;
		psoDesc.SampleDesc.Quality = 0;

		DX12PipelineMG::instance.CreateGraphicPipeline("PIPELINE_DEFERRED_LIGHT", &psoDesc);

		m_lightPSOs[PIPELINELIGHT_DIR].pso = DX12PipelineMG::instance.GetGraphicPipeline("PIPELINE_DEFERRED_LIGHT");
		m_lightPSOs[PIPELINELIGHT_DIR].rootSig = DX12PipelineMG::instance.GetRootSignature("PIPELINE_DEFERRED_LIGHT");

		m_lightPSOs[PIPELINELIGHT_DIR].baseGraphicCmdFunc = [this](ID3D12GraphicsCommandList* cmd)
		{
			cmd->SetGraphicsRootConstantBufferView(ROOT_MAINPASS_CB, GetCurrMainPassCBV());

			for (int i = 0; i < DEFERRED_TEXTURE_RENDERID; i++)
			{
				cmd->SetGraphicsRootShaderResourceView(i+1, m_deferredResources[i]->GetGPUVirtualAddress());
			}

			cmd->SetGraphicsRootShaderResourceView(5, m_swapChain->GetDSResource()->GetGPUVirtualAddress());
		};

		m_lightPSOs[PIPELINELIGHT_DIR].nodeGraphicCmdFunc = [this](ID3D12GraphicsCommandList* cmd, CGHNode* node, unsigned int renderFlag)
		{
			cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_POINTLIST);

			D3D12_VERTEX_BUFFER_VIEW vbView = {};
			/*vbView.BufferLocation = currMesh->meshData[MESHDATA_POSITION]->GetGPUVirtualAddress();
			vbView.SizeInBytes = sizeof(aiVector3D) * currMesh->numData[MESHDATA_POSITION];
			vbView.StrideInBytes = sizeof(aiVector3D);*/

			D3D12_INDEX_BUFFER_VIEW ibView = {};
			/*ibView.BufferLocation = currMesh->meshData[MESHDATA_INDEX]->GetGPUVirtualAddress();
			ibView.Format = DXGI_FORMAT_R32_UINT;
			ibView.SizeInBytes = sizeof(unsigned int) * currMesh->numData[MESHDATA_INDEX];*/

			cmd->IASetVertexBuffers(0, 1, &vbView);
			cmd->IASetIndexBuffer(&ibView);
			/*cmd->DrawIndexedInstanced(currMesh->numData[MESHDATA_INDEX], 1, 0, 0, 0);*/
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
	rDesc.MipLevels = 1;
	rDesc.SampleDesc.Count = 1;
	rDesc.SampleDesc.Quality = 0;
	rDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	rDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

	for (unsigned int i = 0; i < DEFERRED_TEXTURE_NUM; i++)
	{
		m_deferredResources[i] = nullptr;
		rDesc.Format = m_deferredFormat[i];
		ThrowIfFailed(m_d3dDevice->CreateCommittedResource(&prop, D3D12_HEAP_FLAG_NONE, &rDesc,
			D3D12_RESOURCE_STATE_RENDER_TARGET, nullptr, IID_PPV_ARGS(m_deferredResources[i].GetAddressOf())));
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
	}

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

ID3D12CommandAllocator* GraphicDeviceDX12::GetCurrRenderBeginCommandAllocator()
{
	return m_cmdListAllocs[m_currFrame].Get();
}

ID3D12CommandAllocator* GraphicDeviceDX12::GetCurrRenderEndCommandAllocator()
{
	return m_cmdListAllocs[m_currFrame + m_numFrameResource].Get();
}

#include "TestScene.h"
#include "GraphicDeivceDX12.h"
#include "DX12PipelineMG.h"
#include "CGHBaseClass.h"
#include "DirectXColors.h"
#include "DX12TextureBuffer.h"
#include "DX12FontManager.h"

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

TestScene::TestScene()
	: m_meshSet(nullptr)
	, m_materialSet(nullptr)
	, m_textureBuffer(nullptr)
{
}

TestScene::~TestScene()
{
	if (m_textureBuffer)
	{
		delete m_textureBuffer;
	}

	if (m_meshSet)
	{
		delete m_meshSet;
	}

	if (m_materialSet)
	{
		delete m_materialSet;
	}
}

void TestScene::Init()
{
	auto dxGraphic = GraphicDeviceDX12::GetGraphic();
	auto dxDevice = dxGraphic->GetDevice();

	m_meshSet = new CGHMeshDataSet;
	m_materialSet = new CGHMaterialSet;

	dxGraphic->LoadMeshDataFile("MeshData/meshes0.fbx", m_meshSet, m_materialSet);

	//rootsig
	{
		D3D12_ROOT_PARAMETER rootParams[ROOT_NUM] = {};

		rootParams[ROOT_MAINPASS_CB].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		rootParams[ROOT_MAINPASS_CB].Descriptor.RegisterSpace = 0;
		rootParams[ROOT_MAINPASS_CB].Descriptor.ShaderRegister = 0;
		rootParams[ROOT_MAINPASS_CB].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

		rootParams[ROOT_OBJECTINFO_CB].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		rootParams[ROOT_OBJECTINFO_CB].Descriptor.RegisterSpace = 1;
		rootParams[ROOT_OBJECTINFO_CB].Descriptor.ShaderRegister = 0;
		rootParams[ROOT_OBJECTINFO_CB].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

		rootParams[ROOT_MATERIAL_CB].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		rootParams[ROOT_MATERIAL_CB].Descriptor.RegisterSpace = 1;
		rootParams[ROOT_MATERIAL_CB].Descriptor.ShaderRegister = 1;
		rootParams[ROOT_MATERIAL_CB].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

		//gTextureViews
		rootParams[ROOT_TEXTUREINFO_SRV].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
		rootParams[ROOT_TEXTUREINFO_SRV].Descriptor.RegisterSpace = 1;
		rootParams[ROOT_TEXTUREINFO_SRV].Descriptor.ShaderRegister = 0;
		rootParams[ROOT_TEXTUREINFO_SRV].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

		//gMainTextureTable
		D3D12_DESCRIPTOR_RANGE textureTanbleRange = {};
		textureTanbleRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		textureTanbleRange.NumDescriptors = UINT_MAX;
		textureTanbleRange.OffsetInDescriptorsFromTableStart = 0;
		textureTanbleRange.RegisterSpace = 1;
		textureTanbleRange.BaseShaderRegister = 1;

		rootParams[ROOT_TEXTURE_TABLE].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		rootParams[ROOT_TEXTURE_TABLE].DescriptorTable.NumDescriptorRanges = 1;
		rootParams[ROOT_TEXTURE_TABLE].DescriptorTable.pDescriptorRanges = &textureTanbleRange;
		rootParams[ROOT_TEXTURE_TABLE].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

		rootParams[ROOT_NORMAL_SRV].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
		rootParams[ROOT_NORMAL_SRV].Descriptor.RegisterSpace = 2;
		rootParams[ROOT_NORMAL_SRV].Descriptor.ShaderRegister = 0;
		rootParams[ROOT_NORMAL_SRV].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

		rootParams[ROOT_TANGENT_SRV].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
		rootParams[ROOT_TANGENT_SRV].Descriptor.RegisterSpace = 2;
		rootParams[ROOT_TANGENT_SRV].Descriptor.ShaderRegister = 1;
		rootParams[ROOT_TANGENT_SRV].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

		rootParams[ROOT_BITAN_SRV].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
		rootParams[ROOT_BITAN_SRV].Descriptor.RegisterSpace = 2;
		rootParams[ROOT_BITAN_SRV].Descriptor.ShaderRegister = 2;
		rootParams[ROOT_BITAN_SRV].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

		rootParams[ROOT_UV].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
		rootParams[ROOT_UV].Descriptor.RegisterSpace = 2;
		rootParams[ROOT_UV].Descriptor.ShaderRegister = 3;
		rootParams[ROOT_UV].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

		rootParams[ROOT_WEIGHTINFO_SRV].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
		rootParams[ROOT_WEIGHTINFO_SRV].Descriptor.RegisterSpace = 2;
		rootParams[ROOT_WEIGHTINFO_SRV].Descriptor.ShaderRegister = 4;
		rootParams[ROOT_WEIGHTINFO_SRV].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

		rootParams[ROOT_WEIGHT_SRV].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
		rootParams[ROOT_WEIGHT_SRV].Descriptor.RegisterSpace = 2;
		rootParams[ROOT_WEIGHT_SRV].Descriptor.ShaderRegister = 5;
		rootParams[ROOT_WEIGHT_SRV].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

		rootParams[ROOT_BONEDATA_SRV].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
		rootParams[ROOT_BONEDATA_SRV].Descriptor.RegisterSpace = 2;
		rootParams[ROOT_BONEDATA_SRV].Descriptor.ShaderRegister = 6;
		rootParams[ROOT_BONEDATA_SRV].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

		D3D12_ROOT_SIGNATURE_DESC rootsigDesc = {};
		rootsigDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
		rootsigDesc.pStaticSamplers = s_StaticSamplers;
		rootsigDesc.NumStaticSamplers = _countof(s_StaticSamplers);
		rootsigDesc.NumParameters = ROOT_NUM;
		rootsigDesc.pParameters = rootParams;

		DX12PipelineMG::instance.CreateRootSignature("TestScene", &rootsigDesc);
	}

	{
		D3D12_INPUT_ELEMENT_DESC inputElementdesc = { "POSITION" ,0,DXGI_FORMAT_R32G32B32_FLOAT,0,0,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0 };

		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
		psoDesc.NodeMask = 0;

		psoDesc.pRootSignature = DX12PipelineMG::instance.GetRootSignature("TestScene");

		psoDesc.VS = DX12PipelineMG::instance.CreateShader(DX12_SHADER_VERTEX, "TestSceneVS", L"Shader/baseShader.hlsl", "VS");
		psoDesc.PS = DX12PipelineMG::instance.CreateShader(DX12_SHADER_PIXEL, "TestScenePS", L"Shader/baseShader.hlsl", "PS");

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
		psoDesc.NumRenderTargets = 1;
		psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;

		psoDesc.SampleDesc.Count = 1;
		psoDesc.SampleDesc.Quality = 0;

		DX12PipelineMG::instance.CreateGraphicPipeline("TestScene", &psoDesc);
	}

	/*{
		struct DX12Command
		{
			D3D12_DRAW_INDEXED_ARGUMENTS draw;
			D3D12_VERTEX_BUFFER_VIEW vertexBuffer;
			D3D12_INDEX_BUFFER_VIEW indexBuffer;
			D3D12_GPU_VIRTUAL_ADDRESS materialCBV;
			DirectX::XMFLOAT4X4 worldMatC;
			unsigned int numUVComponentC[8] = {};
			unsigned int objectIdC;

			D3D12_GPU_VIRTUAL_ADDRESS boneDataSRV;
			D3D12_GPU_VIRTUAL_ADDRESS normalSRV;
			D3D12_GPU_VIRTUAL_ADDRESS tangentSRV;
			D3D12_GPU_VIRTUAL_ADDRESS bitanSRV;
			D3D12_GPU_VIRTUAL_ADDRESS boneIndexSRV;
			D3D12_GPU_VIRTUAL_ADDRESS weightInfoSRV;
			D3D12_GPU_VIRTUAL_ADDRESS boneWeightSRV;
			D3D12_GPU_VIRTUAL_ADDRESS uvInfoSRV;
			D3D12_GPU_VIRTUAL_ADDRESS uv0SRV;
			D3D12_GPU_VIRTUAL_ADDRESS uv1SRV;
			D3D12_GPU_VIRTUAL_ADDRESS uv2SRV;
			D3D12_GPU_VIRTUAL_ADDRESS textureSRV[8] = {};
		};

		D3D12_INDIRECT_ARGUMENT_DESC argDescs[ROOT_NUM-1];

		D3D12_COMMAND_SIGNATURE_DESC commandSigDesc = {};
	}*/

	{
		m_material = std::make_unique<DX12UploadBuffer<CGHMaterial>>(dxDevice, m_materialSet->materials.size(), true);
		for (unsigned int i = 0; i < m_meshSet->nodes.size(); i++)
		{
			const char* name = m_meshSet->nodes[i].GetaName();

			auto iter = m_nodeKeys.find(name);
			assert(iter == m_nodeKeys.end());

			m_nodeKeys[name] = i;
		}

		for (unsigned int i = 0; i < m_materialSet->materials.size(); i++)
		{
			m_material->CopyData(i, &m_materialSet->materials[i]);
		}

		m_textureBuffer = new DX12TextureBuffer();
		m_textureBuffer->Init(10);
		m_textureBuffer->Open();
		{
			TextureInfo texInfo;
			texInfo.texFilePath = "Textures/BaseBody/cf_m_skin_body_00_MainTex.png";
			texInfo.blend = 1.0f;
			texInfo.uvIndex = 0;
			texInfo.type = aiTextureType_DIFFUSE;
			m_textureBuffer->AddTexture(&texInfo);
			texInfo.texFilePath = "Textures/BaseBody/cf_m_skin_body_00_Texture2.png";
			m_textureBuffer->AddTexture(&texInfo);
			texInfo.texFilePath = "Textures/BaseBody/cf_m_skin_body_00_BumpMap.png";
			m_textureBuffer->AddTexture(&texInfo);
			texInfo.texFilePath = "Textures/BaseBody/cf_m_skin_body_00_BumpMap2.png";
			m_textureBuffer->AddTexture(&texInfo);
			texInfo.texFilePath = "Textures/BaseBody/cf_m_skin_body_00_DetailMainTex.png";
			m_textureBuffer->AddTexture(&texInfo);
			texInfo.texFilePath = "Textures/BaseBody/cf_m_skin_body_00_OcclusionMap.png";
			m_textureBuffer->AddTexture(&texInfo);
			texInfo.texFilePath = "Textures/BaseBody/cf_m_skin_body_00_NailMask.png";
			m_textureBuffer->AddTexture(&texInfo);
		}
		m_textureBuffer->Close();

	
		m_descHeaps.resize(m_meshSet->meshs.size());

		D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
		heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		heapDesc.NodeMask = 0;
		heapDesc.NumDescriptors = 13;
		m_srvSize = dxDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		m_nodeBones.resize(m_meshSet->meshs.size());

		for (size_t i = 0; i < m_meshSet->meshs.size(); i++)
		{
			const CGHMesh& currMesh = m_meshSet->meshs[i];

			m_nodeBones[i] = std::make_unique<DX12UploadBuffer<DirectX::XMMATRIX>>(dxDevice, currMesh.bones.size(), false);

			dxDevice->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(m_descHeaps[i].GetAddressOf()));

			D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
			srvDesc.Format = DXGI_FORMAT_UNKNOWN;
			srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			srvDesc.Buffer.FirstElement = 0;
			srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
			srvDesc.Buffer.NumElements = currMesh.numData[MESHDATA_POSITION];
			srvDesc.Buffer.StructureByteStride = sizeof(aiVector3D);

			auto descHeapHandle = m_descHeaps[i]->GetCPUDescriptorHandleForHeapStart();
			m_textureBuffer->CreateSRVs(descHeapHandle);
		}
	}

	m_commadAllocs.resize(dxGraphic->GetNumFrameResource());

	for (unsigned int i = 0; i < dxGraphic->GetNumFrameResource(); i++)
	{
		dxDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(m_commadAllocs[i].GetAddressOf()));
	}

	dxDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commadAllocs.front().Get(), nullptr, IID_PPV_ARGS(m_commandList.GetAddressOf()));

	ThrowIfFailed(m_commandList->Close());
}

void TestScene::Update(float delta)
{
	m_meshSet->nodes.front().Update(delta);

	for (size_t i = 0; i < m_meshSet->meshs.size(); i++)
	{
		auto& currMesh = m_meshSet->meshs[i];
		auto currBoneUploadBuffer = m_nodeBones[i].get();

		unsigned int numBone = currMesh.bones.size();

		for (unsigned int boneIndex = 0; boneIndex < numBone; boneIndex++)
		{
			auto iter = m_nodeKeys.find(currMesh.bones[boneIndex].name);
			assert(iter != m_nodeKeys.end());

			DirectX::XMMATRIX offsetMat = DirectX::XMLoadFloat4x4(&currMesh.bones[boneIndex].offsetMatrix);
			DirectX::XMMATRIX combinedMat = DirectX::XMLoadFloat4x4(&m_meshSet->nodes[iter->second].m_srt);
			DirectX::XMMATRIX resultMat = offsetMat * combinedMat;

			currBoneUploadBuffer->CopyData(boneIndex, &DirectX::XMMatrixTranspose(resultMat));
		}
	}
}

void TestScene::Render()
{
	auto dxGraphic = GraphicDeviceDX12::GetGraphic();

	auto cmdListAlloc = m_commadAllocs[dxGraphic->GetCurrFrameIndex()].Get();

	ThrowIfFailed(cmdListAlloc->Reset());
	ThrowIfFailed(m_commandList->Reset(cmdListAlloc, nullptr));

	m_commandList->SetPipelineState(DX12PipelineMG::instance.GetGraphicPipeline("TestScene"));
	m_commandList->SetGraphicsRootSignature(DX12PipelineMG::instance.GetRootSignature("TestScene"));

	m_commandList->RSSetScissorRects(1, &dxGraphic->GetBaseScissorRect());
	m_commandList->RSSetViewports(1, &dxGraphic->GetBaseViewport());

	D3D12_CPU_DESCRIPTOR_HANDLE presentRTV[] = { dxGraphic->GetCurrPresentRTV() };
	auto presentDSV = dxGraphic->GetPresentDSV();
	m_commandList->OMSetRenderTargets(1, presentRTV, false, &presentDSV);
	m_commandList->SetGraphicsRootConstantBufferView(ROOT_MAINPASS_CB, dxGraphic->GetCurrMainPassCBV());

	D3D12_GPU_VIRTUAL_ADDRESS matCB = m_material->Resource()->GetGPUVirtualAddress();
	unsigned int matStride = m_material->GetElementByteSize();
	unsigned int objectInfoStride = m_objectInfos->GetElementByteSize();

	for (unsigned int i = 0; i < m_meshSet->meshs.size(); i++)
	{
		auto& currMesh = m_meshSet->meshs[i];

		switch (currMesh.primitiveType)
		{
		case aiPrimitiveType_POINT:
			m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_POINTLIST);
			break;
		case aiPrimitiveType_LINE:
			m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);
			break;
		case aiPrimitiveType_TRIANGLE:
			m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			break;
		default:
			assert(false);
			break;
		}

	

		auto descHeapHandle = m_descHeaps[i]->GetGPUDescriptorHandleForHeapStart();

		ID3D12DescriptorHeap* heaps[] = { m_descHeaps[i].Get() };
		m_commandList->SetDescriptorHeaps(_countof(heaps), heaps);

		m_commandList->SetGraphicsRootConstantBufferView(ROOT_MATERIAL_CB, matCB + (matStride * currMesh.materialIndex));
		m_commandList->SetGraphicsRootShaderResourceView(ROOT_TEXTUREINFO_SRV, m_textureBuffer->GetTextureInfos());
		m_commandList->SetGraphicsRootDescriptorTable(ROOT_TEXTURE_TABLE, descHeapHandle);

		D3D12_VERTEX_BUFFER_VIEW vbView = {};
		vbView.BufferLocation = currMesh.meshData[MESHDATA_POSITION]->GetGPUVirtualAddress();
		vbView.SizeInBytes = sizeof(aiVector3D) * currMesh.numData[MESHDATA_POSITION];
		vbView.StrideInBytes = sizeof(aiVector3D);

		D3D12_INDEX_BUFFER_VIEW ibView = {};
		ibView.BufferLocation = currMesh.meshData[MESHDATA_INDEX]->GetGPUVirtualAddress();
		ibView.Format = DXGI_FORMAT_R32_UINT;
		ibView.SizeInBytes = sizeof(unsigned int) * currMesh.numData[MESHDATA_INDEX];

		m_commandList->IASetVertexBuffers(0, 1, &vbView);
		m_commandList->IASetIndexBuffer(&ibView);

		m_commandList->SetGraphicsRootShaderResourceView(ROOT_NORMAL_SRV, currMesh.meshData[MESHDATA_NORMAL]->GetGPUVirtualAddress());
		m_commandList->SetGraphicsRootShaderResourceView(ROOT_TANGENT_SRV, currMesh.meshData[MESHDATA_TAN]->GetGPUVirtualAddress());
		m_commandList->SetGraphicsRootShaderResourceView(ROOT_BITAN_SRV, currMesh.meshData[MESHDATA_BITAN]->GetGPUVirtualAddress());
		m_commandList->SetGraphicsRootShaderResourceView(ROOT_UV, currMesh.meshDataUVs[0]->GetGPUVirtualAddress());

		m_commandList->SetGraphicsRootShaderResourceView(ROOT_BONEDATA_SRV, m_nodeBones[i]->Resource()->GetGPUVirtualAddress());
		m_commandList->SetGraphicsRootShaderResourceView(ROOT_WEIGHTINFO_SRV, currMesh.boneWeightInfos->GetGPUVirtualAddress());
		m_commandList->SetGraphicsRootShaderResourceView(ROOT_WEIGHT_SRV, currMesh.boneWeights->GetGPUVirtualAddress());

		m_commandList->DrawIndexedInstanced(currMesh.numData[MESHDATA_INDEX], 1, 0, 0, 0);
	}

	ThrowIfFailed(m_commandList->Close());
	ID3D12CommandList* cmdsLists[] = { m_commandList.Get() };
	dxGraphic->GetCommandQueue()->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);
}

void TestScene::CreateFontPSO()
{
	auto device = GraphicDeviceDX12::GetDevice();

	{
		enum FONT_ROOTSIG
		{
			ORTHOMAT,
			OFFSET_CHAR,
			FONT_MESHINFO_INDEX,
			STRING_INFO_SRV,
			CHAR_STREAM_SRV,
			FONT_MESHINFO_SRV,
			FONT_ROOT_COUNT
		};

		D3D12_ROOT_PARAMETER rootParams[FONT_ROOT_COUNT] = {};
		rootParams[ORTHOMAT].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
		rootParams[ORTHOMAT].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
		rootParams[ORTHOMAT].Constants.Num32BitValues = 16;
		rootParams[ORTHOMAT].Constants.RegisterSpace = 0;
		rootParams[ORTHOMAT].Constants.ShaderRegister = 0;

		rootParams[OFFSET_CHAR].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
		rootParams[OFFSET_CHAR].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
		rootParams[OFFSET_CHAR].Constants.Num32BitValues = 1;
		rootParams[OFFSET_CHAR].Constants.RegisterSpace = 0;
		rootParams[OFFSET_CHAR].Constants.ShaderRegister = 1;

		rootParams[FONT_MESHINFO_INDEX].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
		rootParams[FONT_MESHINFO_INDEX].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
		rootParams[FONT_MESHINFO_INDEX].Constants.Num32BitValues = 1;
		rootParams[FONT_MESHINFO_INDEX].Constants.RegisterSpace = 0;
		rootParams[FONT_MESHINFO_INDEX].Constants.ShaderRegister = 2;

		rootParams[STRING_INFO_SRV].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
		rootParams[STRING_INFO_SRV].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
		rootParams[STRING_INFO_SRV].Descriptor.RegisterSpace = 0;
		rootParams[STRING_INFO_SRV].Descriptor.ShaderRegister = 0;

		rootParams[CHAR_STREAM_SRV].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
		rootParams[CHAR_STREAM_SRV].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
		rootParams[CHAR_STREAM_SRV].Descriptor.RegisterSpace = 0;
		rootParams[CHAR_STREAM_SRV].Descriptor.ShaderRegister = 1;

		rootParams[FONT_MESHINFO_SRV].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
		rootParams[FONT_MESHINFO_SRV].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
		rootParams[FONT_MESHINFO_SRV].Descriptor.RegisterSpace = 0;
		rootParams[FONT_MESHINFO_SRV].Descriptor.ShaderRegister = 2;

		D3D12_ROOT_SIGNATURE_DESC rootDesc = {};
		rootDesc.NumStaticSamplers = _countof(s_StaticSamplers);
		rootDesc.pStaticSamplers = s_StaticSamplers;
		rootDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
		rootDesc.NumParameters = _countof(rootParams);
		rootDesc.pParameters = rootParams;

		DX12PipelineMG::instance.CreateRootSignature("font", &rootDesc);
	}

	{
		D3D12_INPUT_ELEMENT_DESC elementDesc = {};
		elementDesc.AlignedByteOffset = 0;
		elementDesc.Format = DXGI_FORMAT_R32G32_FLOAT;
		elementDesc.InputSlot = 0;
		elementDesc.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
		elementDesc.InstanceDataStepRate = 0;
		elementDesc.SemanticIndex = 0;
		elementDesc.SemanticName = "POSITION";

		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = { };
		psoDesc.pRootSignature = DX12PipelineMG::instance.GetRootSignature("font");

		psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
		psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		psoDesc.InputLayout.NumElements = 1;
		psoDesc.InputLayout.pInputElementDescs = &elementDesc;
		psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
		psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
		psoDesc.NumRenderTargets = 1;
		psoDesc.SampleMask = UINT_MAX;
		psoDesc.SampleDesc.Count = 1;
		psoDesc.SampleDesc.Quality = 0;

		DX12PipelineMG::instance.CreateGraphicPipeline("font", &psoDesc);
	}
}

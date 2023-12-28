#include "TestScene.h"
#include "GraphicDeivceDX12.h"
#include "DX12PipelineMG.h"
#include "CGHBaseClass.h"
#include "CGHGraphicResource.h"
#include "DirectXColors.h"

enum
{
	ROOT_MAINPASS_CB = 0,
	ROOT_MATERIAL_CB,
	ROOT_OBJECTINFO_CB,
	ROOT_BONEDATA_SRV,
	ROOT_NORMAL_SRV,
	ROOT_TANGENT_SRV,
	ROOT_BITAN_SRV,
	ROOT_BONEINDEX_SRV,
	ROOT_WEIGHTINFO_SRV,
	ROOT_WEIGHT_SRV,
	ROOT_UVINFO_SRV,
	ROOT_UV0_SRV,
	ROOT_UV1_SRV,
	ROOT_UV2_SRV,
	//ROOT_TEXTUREVIEW_SRV,
	//ROOT_TEXTURE_TABLE,
	ROOT_NUM,
};

TestScene::TestScene()
	: m_meshSet(nullptr)
	, m_materialSet(nullptr)
{

}

TestScene::~TestScene()
{
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

	dxGraphic->LoadMeshDataFile("./../Common/MeshData/meshes0.fbx", m_meshSet, m_materialSet);

	//rootsig
	{
		D3D12_ROOT_PARAMETER rootParams[ROOT_NUM] = {};

		rootParams[ROOT_MAINPASS_CB].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		rootParams[ROOT_MAINPASS_CB].Descriptor.RegisterSpace = 0;
		rootParams[ROOT_MAINPASS_CB].Descriptor.ShaderRegister = 0;
		rootParams[ROOT_MAINPASS_CB].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

		rootParams[ROOT_MATERIAL_CB].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		rootParams[ROOT_MATERIAL_CB].Descriptor.RegisterSpace = 0;
		rootParams[ROOT_MATERIAL_CB].Descriptor.ShaderRegister = 1;
		rootParams[ROOT_MATERIAL_CB].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

		rootParams[ROOT_OBJECTINFO_CB].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		rootParams[ROOT_OBJECTINFO_CB].Descriptor.RegisterSpace = 0;
		rootParams[ROOT_OBJECTINFO_CB].Descriptor.ShaderRegister = 2;
		rootParams[ROOT_OBJECTINFO_CB].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

		rootParams[ROOT_BONEDATA_SRV].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
		rootParams[ROOT_BONEDATA_SRV].Descriptor.RegisterSpace = 0;
		rootParams[ROOT_BONEDATA_SRV].Descriptor.ShaderRegister = 0;
		rootParams[ROOT_BONEDATA_SRV].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

		rootParams[ROOT_NORMAL_SRV].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
		rootParams[ROOT_NORMAL_SRV].Descriptor.RegisterSpace = 0;
		rootParams[ROOT_NORMAL_SRV].Descriptor.ShaderRegister = 1;
		rootParams[ROOT_NORMAL_SRV].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

		rootParams[ROOT_TANGENT_SRV].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
		rootParams[ROOT_TANGENT_SRV].Descriptor.RegisterSpace = 0;
		rootParams[ROOT_TANGENT_SRV].Descriptor.ShaderRegister = 2;
		rootParams[ROOT_TANGENT_SRV].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

		rootParams[ROOT_BITAN_SRV].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
		rootParams[ROOT_BITAN_SRV].Descriptor.RegisterSpace = 0;
		rootParams[ROOT_BITAN_SRV].Descriptor.ShaderRegister = 3;
		rootParams[ROOT_BITAN_SRV].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

		rootParams[ROOT_BONEINDEX_SRV].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
		rootParams[ROOT_BONEINDEX_SRV].Descriptor.RegisterSpace = 0;
		rootParams[ROOT_BONEINDEX_SRV].Descriptor.ShaderRegister = 4;
		rootParams[ROOT_BONEINDEX_SRV].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

		rootParams[ROOT_WEIGHTINFO_SRV].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
		rootParams[ROOT_WEIGHTINFO_SRV].Descriptor.RegisterSpace = 0;
		rootParams[ROOT_WEIGHTINFO_SRV].Descriptor.ShaderRegister = 5;
		rootParams[ROOT_WEIGHTINFO_SRV].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

		rootParams[ROOT_WEIGHT_SRV].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
		rootParams[ROOT_WEIGHT_SRV].Descriptor.RegisterSpace = 0;
		rootParams[ROOT_WEIGHT_SRV].Descriptor.ShaderRegister = 6;
		rootParams[ROOT_WEIGHT_SRV].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

		rootParams[ROOT_UVINFO_SRV].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
		rootParams[ROOT_UVINFO_SRV].Descriptor.RegisterSpace = 0;
		rootParams[ROOT_UVINFO_SRV].Descriptor.ShaderRegister = 7;
		rootParams[ROOT_UVINFO_SRV].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

		rootParams[ROOT_UV0_SRV].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
		rootParams[ROOT_UV0_SRV].Descriptor.RegisterSpace = 0;
		rootParams[ROOT_UV0_SRV].Descriptor.ShaderRegister = 8;
		rootParams[ROOT_UV0_SRV].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

		rootParams[ROOT_UV1_SRV].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
		rootParams[ROOT_UV1_SRV].Descriptor.RegisterSpace = 0;
		rootParams[ROOT_UV1_SRV].Descriptor.ShaderRegister = 9;
		rootParams[ROOT_UV1_SRV].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

		rootParams[ROOT_UV2_SRV].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
		rootParams[ROOT_UV2_SRV].Descriptor.RegisterSpace = 0;
		rootParams[ROOT_UV2_SRV].Descriptor.ShaderRegister = 10;
		rootParams[ROOT_UV2_SRV].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

		//gTextureViews
		//rootParams[ROOT_TEXTUREVIEW_SRV].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
		//rootParams[ROOT_TEXTUREVIEW_SRV].Descriptor.RegisterSpace = 1;
		//rootParams[ROOT_TEXTUREVIEW_SRV].Descriptor.ShaderRegister = 0;
		//rootParams[ROOT_TEXTUREVIEW_SRV].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

		////gMainTextureTable
		//D3D12_DESCRIPTOR_RANGE textureTanbleRange = {};
		//textureTanbleRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		//textureTanbleRange.NumDescriptors = UINT_MAX;
		//textureTanbleRange.OffsetInDescriptorsFromTableStart = 0;
		//textureTanbleRange.RegisterSpace = 1;
		//textureTanbleRange.BaseShaderRegister = 1;

		//rootParams[ROOT_TEXTURE_TABLE].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
		//rootParams[ROOT_TEXTURE_TABLE].DescriptorTable.NumDescriptorRanges = 1;
		//rootParams[ROOT_TEXTURE_TABLE].DescriptorTable.pDescriptorRanges = &textureTanbleRange;
		//rootParams[ROOT_TEXTURE_TABLE].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

		D3D12_ROOT_SIGNATURE_DESC rootsigDesc = {};
		rootsigDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
		rootsigDesc.pStaticSamplers = s_StaticSamplers;
		rootsigDesc.NumStaticSamplers = _countof(s_StaticSamplers);
		rootsigDesc.NumParameters = ROOT_NUM;
		rootsigDesc.pParameters = rootParams;

		s_pipelineMG.CreateRootSignature("TestScene", &rootsigDesc);
	}

	{
		D3D12_INPUT_ELEMENT_DESC inputElementdesc = { "POSITION" ,0,DXGI_FORMAT_R32G32B32_FLOAT,0,0,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0 };

		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
		psoDesc.NodeMask = 0;
		psoDesc.pRootSignature = s_pipelineMG.GetRootSignature("TestScene");
		psoDesc.VS = s_pipelineMG.CreateShader(DX12_SHADER_VERTEX, "TestSceneVS", L"baseShader.hlsl", "VS");
		psoDesc.PS = s_pipelineMG.CreateShader(DX12_SHADER_PIXEL, "TestScenePS", L"baseShader.hlsl", "PS");
		psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		psoDesc.InputLayout.NumElements = 1;
		psoDesc.InputLayout.pInputElementDescs = &inputElementdesc;

		psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
		psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;

		psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
		psoDesc.NumRenderTargets = 1;
		psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;

		psoDesc.SampleMask = UINT_MAX;
		psoDesc.SampleDesc.Count = 1;
		psoDesc.SampleDesc.Quality = 0;

		s_pipelineMG.CreateGraphicPipeline("TestScene", &psoDesc);
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
		std::unordered_map<std::string, unsigned int> nodeKeys;
		for (unsigned int i = 0; i < m_meshSet->nodes.size(); i++)
		{
			const char* name = m_meshSet->nodes[i].GetaName();

			auto iter = nodeKeys.find(name);
			assert(iter == nodeKeys.end());

			nodeKeys[name] = i;
		}

		for (auto& currMesh : m_meshSet->meshs)
		{
			m_boneIndices.emplace_back(dxDevice, currMesh.bones.size(), false);

			auto& currBuffer = m_boneIndices.back();

			for (unsigned int i = 0; i < currMesh.bones.size(); i++)
			{
				auto iter = nodeKeys.find(currMesh.bones[i].name);
				assert(iter != nodeKeys.end());

				currBuffer.CopyData(i, iter->second);
			}
		}

		m_nodeBones = std::make_unique<DX12UploadBuffer < DirectX::XMFLOAT4X4>>(dxDevice, m_meshSet->nodes.size(), false);
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
	for (unsigned int i = 0; i < m_meshSet->nodes.size(); i++)
	{
		m_nodeBones->CopyData(i, m_meshSet->nodes[i].m_srt);
	}
}

void TestScene::Render()
{
	auto dxGraphic = GraphicDeviceDX12::GetGraphic();

	auto cmdListAlloc = m_commadAllocs[dxGraphic->GetCurrFrameIndex()].Get();
	
	ThrowIfFailed(cmdListAlloc->Reset());
	ThrowIfFailed(m_commandList->Reset(cmdListAlloc, nullptr));

	m_commandList->SetPipelineState(s_pipelineMG.GetGraphicPipeline("TestScene"));
	m_commandList->SetGraphicsRootSignature(s_pipelineMG.GetRootSignature("TestScene"));

	m_commandList->RSSetScissorRects(1, &dxGraphic->GetBaseScissorRect());
	m_commandList->RSSetViewports(1, &dxGraphic->GetBaseViewport());

	D3D12_CPU_DESCRIPTOR_HANDLE presentRTV[] = { dxGraphic->GetCurrPresentRTV() };
	auto presentDSV = dxGraphic->GetPresentDSV();
	m_commandList->OMSetRenderTargets(1, presentRTV, false, &presentDSV);
	m_commandList->SetGraphicsRootConstantBufferView(ROOT_MAINPASS_CB, dxGraphic->GetCurrMainPassCBV());

	D3D12_GPU_VIRTUAL_ADDRESS matCB = m_materialSet->materialDatas->Resource()->GetGPUVirtualAddress();
	unsigned int matStride = m_materialSet->materialDatas->GetElementByteSize();

	for (unsigned int i =0 ;i< m_meshSet->meshs.size(); i++)
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

		D3D12_VERTEX_BUFFER_VIEW vbView = {};
		vbView.BufferLocation = currMesh.meshData[MESHDATA_POSITION]->GetGPUVirtualAddress();
		vbView.SizeInBytes = sizeof(aiVector3D) * currMesh.numData[MESHDATA_POSITION];
		vbView.StrideInBytes = sizeof(aiVector3D);

		D3D12_INDEX_BUFFER_VIEW ibView = {};
		ibView.BufferLocation= currMesh.meshData[MESHDATA_INDEX]->GetGPUVirtualAddress();
		ibView.Format = DXGI_FORMAT_R32_UINT;
		ibView.SizeInBytes = sizeof(unsigned int) * currMesh.numData[MESHDATA_INDEX];

		m_commandList->IASetVertexBuffers(0, 1, &vbView);
		m_commandList->IASetIndexBuffer(&ibView);

		m_commandList->SetGraphicsRootConstantBufferView(ROOT_MATERIAL_CB, matCB + (matStride * currMesh.materialIndex));
		m_commandList->SetGraphicsRootConstantBufferView(ROOT_OBJECTINFO_CB, );
		m_commandList->SetGraphicsRootShaderResourceView(ROOT_BONEDATA_SRV, m_nodeBones->Resource()->GetGPUVirtualAddress());
		m_commandList->SetGraphicsRootShaderResourceView(ROOT_NORMAL_SRV, currMesh.meshData[MESHDATA_NORMAL]->GetGPUVirtualAddress());
		m_commandList->SetGraphicsRootShaderResourceView(ROOT_TANGENT_SRV, currMesh.meshData[MESHDATA_TAN]->GetGPUVirtualAddress());
		m_commandList->SetGraphicsRootShaderResourceView(ROOT_BITAN_SRV, currMesh.meshData[MESHDATA_BITAN]->GetGPUVirtualAddress());
		m_commandList->SetGraphicsRootShaderResourceView(ROOT_BONEINDEX_SRV, m_boneIndices[i].Resource()->GetGPUVirtualAddress());
		m_commandList->SetGraphicsRootShaderResourceView(ROOT_WEIGHTINFO_SRV, currMesh.boneWeightInfos->GetGPUVirtualAddress());
		m_commandList->SetGraphicsRootShaderResourceView(ROOT_WEIGHT_SRV, currMesh.boneWeights->GetGPUVirtualAddress());
		m_commandList->SetGraphicsRootShaderResourceView(ROOT_UVINFO_SRV, currMesh.UVdataInfos->GetGPUVirtualAddress());

		m_commandList->SetGraphicsRootShaderResourceView(ROOT_UV0_SRV, currMesh.meshDataUV[0]->GetGPUVirtualAddress());

		for (unsigned int j = ROOT_UV1_SRV; j <= ROOT_UV2_SRV; j++)
		{
			unsigned int currIndex = j - ROOT_UV0_SRV;
			if (currMesh.meshDataUV[currIndex].Get() != nullptr)
			{
				m_commandList->SetGraphicsRootShaderResourceView(j, currMesh.meshDataUV[currIndex]->GetGPUVirtualAddress());
			}
			else
			{
				m_commandList->SetGraphicsRootShaderResourceView(j, currMesh.meshDataUV[0]->GetGPUVirtualAddress());
			}
		}

		m_commandList->DrawIndexedInstanced(currMesh.numData[MESHDATA_INDEX], 1, 0, 0, 0);
	}

	ThrowIfFailed(m_commandList->Close());
	ID3D12CommandList* cmdsLists[] = { m_commandList.Get() };
	dxGraphic->GetCommandQueue()->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);
}

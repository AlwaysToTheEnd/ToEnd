#include <typeinfo>
#include "Component.h"
#include "CGHBaseClass.h"
#include "GraphicDeivceDX12.h"

size_t COMTransform::s_hashCode = typeid(COMTransform).hash_code();
size_t COMSkinnedMesh::s_hashCode = typeid(COMSkinnedMesh).hash_code();
size_t COMDX12SkinnedMeshRenderer::s_hashCode = typeid(COMDX12SkinnedMeshRenderer).hash_code();

COMTransform::COMTransform(CGHNode* node)
{
}

void COMTransform::Update(CGHNode* node, unsigned int, float delta)
{
	DirectX::XMMATRIX rotateMat = DirectX::XMMatrixRotationQuaternion(DirectX::XMLoadFloat4(&m_queternion));
	DirectX::XMMATRIX scaleMat = DirectX::XMMatrixScaling(m_scale.x, m_scale.y, m_scale.z);
	DirectX::XMMATRIX transMat = DirectX::XMMatrixTranslation(m_pos.x, m_pos.y, m_pos.z); 

	CGHNode* parentNode = node->GetParent();
	if (parentNode != nullptr)
	{
		DirectX::XMStoreFloat4x4(&node->m_srt, scaleMat * rotateMat * transMat * DirectX::XMLoadFloat4x4(&parentNode->m_srt));
	}
	else
	{
		DirectX::XMStoreFloat4x4(&node->m_srt, scaleMat * rotateMat * transMat);
	}
}

void XM_CALLCONV COMTransform::SetPos(DirectX::FXMVECTOR pos)
{
	DirectX::XMStoreFloat3(&m_pos, pos);
}

void XM_CALLCONV COMTransform::SetScale(DirectX::FXMVECTOR scale)
{
	DirectX::XMStoreFloat3(&m_scale, scale);
}

void XM_CALLCONV COMTransform::SetRotateQuter(DirectX::FXMVECTOR quterRotate)
{
	DirectX::XMStoreFloat4(&m_queternion, quterRotate);
}

COMSkinnedMesh::COMSkinnedMesh(CGHNode* node)
{
	auto graphic = GraphicDeviceDX12::GetGraphic();
	auto device = GraphicDeviceDX12::GetDevice();
	unsigned int frameNum = graphic->GetNumFrameResource();

	DirectX::XMFLOAT4X4* mapped = nullptr;
	ThrowIfFailed(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(BONEDATASIZE * frameNum),
		D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
		IID_PPV_ARGS(m_boneDatas.GetAddressOf())));
	ThrowIfFailed(m_boneDatas->Map(0, nullptr, reinterpret_cast<void**>(&mapped)));

	node->m_childNodeTreeChangeEvent.push_back({ std::bind(&COMSkinnedMesh::NodeTreeChanged, this), this });

	for (unsigned int i = 0; i < frameNum; i++)
	{
		m_boneDatasCpu.push_back(mapped);
		
		for (unsigned int j = 0; j < MAXNUMBONE; j++)
		{
			std::memcpy(mapped, &CGH::IdentityMatrix, sizeof(DirectX::XMFLOAT4X4));
			mapped++;
		}
	}
}

COMSkinnedMesh::~COMSkinnedMesh()
{
}

void COMSkinnedMesh::Release(CGHNode* node)
{
	m_boneDatas->Unmap(0, nullptr);

	auto& events = node->m_childNodeTreeChangeEvent;
	
	for (size_t i = 0; i < events.size(); i++)
	{
		if (events[i].object == this)
		{
			events.back() = events[i];
			events.pop_back();
			break;
		}
	}
}

void COMSkinnedMesh::RateUpdate(CGHNode* node, unsigned int currFrame, float delta)
{
	if (m_currNodeTreeDirtyFlag)
	{
		m_currNodeTree.clear();
		std::vector<const CGHNode*> childNodeStack;
		childNodeStack.reserve(256);
		node->GetChildNodes(&childNodeStack);

		for (auto iter : childNodeStack)
		{
			m_currNodeTree.insert({ iter->GetaName(), iter });
		}

		m_currNodeTreeDirtyFlag = false;
	}

	if (m_data)
	{
		DirectX::XMFLOAT4X4* mapped = m_boneDatasCpu[currFrame];

		for (auto& currMesh : m_data->meshs)
		{
			for (auto& currBone : currMesh.bones)
			{
				auto iter = m_currNodeTree.find(currBone.name);
				if (iter != m_currNodeTree.end())
				{
					DirectX::XMStoreFloat4x4(mapped, DirectX::XMLoadFloat4x4(&currBone.offsetMatrix) * DirectX::XMLoadFloat4x4(&iter->second->m_srt));
				}

				mapped++;
			}
		}
	}
}

void COMSkinnedMesh::SetMeshData(const CGHMeshDataSet* meshData)
{
	if (m_data == meshData) return;

	m_data = meshData;

	if (m_data)
	{
		unsigned int numBone = 0;
		for (auto& iter : m_data->meshs)
		{
			numBone += iter.bones.size();
		}

		assert(numBone < MAXNUMBONE);
	}
}

D3D12_GPU_VIRTUAL_ADDRESS COMSkinnedMesh::GetBoneData(unsigned int currFrame)
{
	auto result = m_boneDatas->GetGPUVirtualAddress();
	result += BONEDATASIZE * currFrame;

	return result;
}

//unsigned int COMDX12SkinnedMeshRenderer::Render(ID3D12GraphicsCommandList* cmd, unsigned int rootsigOffset)
//{
//	if (m_meshSet)
//	{
//		for (unsigned int i = 0; i < m_meshSet->meshs.size(); i++)
//		{
//			auto& currMesh = m_meshSet->meshs[i];
//
//			D3D12_VERTEX_BUFFER_VIEW vbView = {};
//			vbView.BufferLocation = currMesh.meshData[MESHDATA_POSITION]->GetGPUVirtualAddress();
//			vbView.SizeInBytes = sizeof(aiVector3D) * currMesh.numData[MESHDATA_POSITION];
//			vbView.StrideInBytes = sizeof(aiVector3D);
//
//			D3D12_INDEX_BUFFER_VIEW ibView = {};
//			ibView.BufferLocation = currMesh.meshData[MESHDATA_INDEX]->GetGPUVirtualAddress();
//			ibView.Format = DXGI_FORMAT_R32_UINT;
//			ibView.SizeInBytes = sizeof(unsigned int) * currMesh.numData[MESHDATA_INDEX];
//
//			cmd->IASetVertexBuffers(0, 1, &vbView);
//			cmd->IASetIndexBuffer(&ibView);
//
//			cmd->SetGraphicsRootShaderResourceView(rootsigOffset, currMesh.meshData[MESHDATA_NORMAL]->GetGPUVirtualAddress());
//			cmd->SetGraphicsRootShaderResourceView(rootsigOffset + 1, currMesh.meshData[MESHDATA_TAN]->GetGPUVirtualAddress());
//			cmd->SetGraphicsRootShaderResourceView(rootsigOffset + 2, currMesh.meshData[MESHDATA_BITAN]->GetGPUVirtualAddress());
//			cmd->SetGraphicsRootShaderResourceView(rootsigOffset + 3, currMesh.meshDataUVs[0]->GetGPUVirtualAddress());
//
//			cmd->SetGraphicsRootShaderResourceView(rootsigOffset + 4, currMesh.boneWeightInfos->GetGPUVirtualAddress());
//			cmd->SetGraphicsRootShaderResourceView(rootsigOffset + 5, currMesh.boneWeights->GetGPUVirtualAddress());
//			cmd->SetGraphicsRootShaderResourceView(rootsigOffset + 6, m_nodeBones[i]->Resource()->GetGPUVirtualAddress());
//
//			cmd->DrawIndexedInstanced(currMesh.numData[MESHDATA_INDEX], 1, 0, 0, 0);
//		}
//	}
//
//	return rootsigOffset + 7;
//}

void COMDX12SkinnedMeshRenderer::RateUpdate(CGHNode* node, unsigned int currFrame, float delta)
{
	if (node->GetComponent<COMSkinnedMesh>())
	{
		GraphicDeviceDX12::GetGraphic()->RenderSkinnedMesh(node, renderFlag);
	}
}

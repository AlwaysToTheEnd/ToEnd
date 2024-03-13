#include <typeinfo>
#include "Component.h"
#include "CGHBaseClass.h"
#include "GraphicDeivceDX12.h"

size_t COMTransform::s_hashCode = typeid(COMTransform).hash_code();
size_t COMSkinnedMesh::s_hashCode = typeid(COMSkinnedMesh).hash_code();

COMTransform::COMTransform()
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

COMSkinnedMesh::COMSkinnedMesh()
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
	m_boneDatas->Unmap(0, nullptr);
}

void COMSkinnedMesh::RateUpdate(CGHNode* node, unsigned int currFrame, float delta)
{
	const std::unordered_map<std::string, CGHNode*>* nodeMap = node->GetNodeTree();

	if (m_data)
	{
		DirectX::XMFLOAT4X4* mapped = m_boneDatasCpu[currFrame];

		for (auto& currMesh : m_data->meshs)
		{
			for (auto& currBone : currMesh.bones)
			{
				auto iter = nodeMap->find(currBone.name);

				if (iter != nodeMap->end())
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

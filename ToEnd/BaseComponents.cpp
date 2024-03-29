#include "BaseComponents.h"
#include <typeinfo>
#include "CGHBaseClass.h"
#include "GraphicDeivceDX12.h"
#include "DX12GraphicResourceManager.h"
#include "DX12TextureBuffer.h"

size_t COMTransform::s_hashCode = typeid(COMTransform).hash_code();
size_t COMMaterial::s_hashCode = typeid(COMMaterial).hash_code();
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

	node->AddEvent(std::bind(&COMSkinnedMesh::NodeTreeDirty, this), CGHNODE_EVENT_FLAG_CHILDTREE_CHANGED);

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
}

void COMSkinnedMesh::Release(CGHNode* node)
{
	m_boneDatas->Unmap(0, nullptr);
}

void COMSkinnedMesh::RateUpdate(CGHNode* node, unsigned int currFrame, float delta)
{
	if (m_nodeTreeDirty)
	{
		m_currNodeTree.clear();
		std::vector<const CGHNode*> childNodeStack;
		childNodeStack.reserve(512);
		node->GetChildNodes(&childNodeStack);

		for (auto iter : childNodeStack)
		{
			m_currNodeTree.insert({ iter->GetaName(), iter });
		}

		m_nodeTreeDirty = false;
	}

	if (m_data)
	{
		DirectX::XMFLOAT4X4* mapped = m_boneDatasCpu[currFrame];

		for (auto& currBone : m_data->bones)
		{
			auto iter = m_currNodeTree.find(currBone.name);
			if (iter != m_currNodeTree.end())
			{
				DirectX::XMMATRIX mat = DirectX::XMLoadFloat4x4(&currBone.offsetMatrix) * DirectX::XMLoadFloat4x4(&iter->second->m_srt);
				DirectX::XMStoreFloat4x4(mapped, DirectX::XMMatrixTranspose(mat));
			}
			else
			{
				assert(false);
			}

			mapped++;
		}
	}
}

void COMSkinnedMesh::SetMeshData(const CGHMesh* meshData)
{
	if (m_data == meshData) return;

	m_data = meshData;

	if (m_data)
	{
		unsigned int numBone = 0;

		assert(m_data->bones.size() < MAXNUMBONE);
	}
}

D3D12_GPU_VIRTUAL_ADDRESS COMSkinnedMesh::GetBoneData(unsigned int currFrame)
{
	auto result = m_boneDatas->GetGPUVirtualAddress();
	result += BONEDATASIZE * currFrame;

	return result;
}

void COMSkinnedMesh::NodeTreeDirty()
{
	m_nodeTreeDirty = true;
}

COMDX12SkinnedMeshRenderer::COMDX12SkinnedMeshRenderer(CGHNode* node)
{
}

void COMDX12SkinnedMeshRenderer::RateUpdate(CGHNode* node, unsigned int currFrame, float delta)
{
	if (node->GetComponent<COMSkinnedMesh>())
	{
		GraphicDeviceDX12::GetGraphic()->RenderSkinnedMesh(node, m_renderFlag);
	}
}

COMMaterial::COMMaterial(CGHNode* node)
{
	m_material = DX12GraphicResourceManager::s_insatance.CreateData<CGHMaterial>(&m_currMaterialIndex);

	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
	heapDesc.NumDescriptors = CGHMaterial::CGHMaterialTextureNum;
	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	ThrowIfFailed(GraphicDeviceDX12::GetDevice()->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(m_descHeap.GetAddressOf())));

	m_textureBuffer = new DX12TextureBuffer;
	m_textureBuffer->SetBufferSize(CGHMaterial::CGHMaterialTextureNum);
}

COMMaterial::~COMMaterial()
{
}

void COMMaterial::Release(CGHNode* node)
{
	if (m_material)
	{
		DX12GraphicResourceManager::s_insatance.ReleaseData(m_material);
		m_material = nullptr;
	}

	if (m_textureBuffer)
	{
		delete m_textureBuffer;
	}
}

void COMMaterial::SetData(const CGHMaterial* material)
{
	*m_material = *material;
}

void COMMaterial::SetTexture(const TextureInfo* textureInfo, unsigned int index)
{
	if (textureInfo)
	{
		m_textureBuffer->SetTexture(TextureInfo::GetTexturePath(textureInfo->textureFilePathID).c_str(), index);
		m_material->textureInfo[index] = *textureInfo;

		m_textureBuffer->CreateSRVs(m_descHeap->GetCPUDescriptorHandleForHeapStart());
	}
	else
	{
		m_material->textureInfo[index].type = aiTextureType_NONE;
	}
}

UINT64 COMMaterial::GetMaterialDataGPU()
{
	UINT64 result = 0;

	if (m_material)
	{
		result = DX12GraphicResourceManager::s_insatance.GetGpuAddress<CGHMaterial>(m_currMaterialIndex);
	}

	return result;
}

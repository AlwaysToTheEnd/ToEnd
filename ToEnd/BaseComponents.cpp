#include "BaseComponents.h"
#include "CGHBaseClass.h"
#include "GraphicDeivceDX12.h"
#include "DX12GraphicResourceManager.h"
#include "DX12TextureBuffer.h"
#include "Dx12FontManager.h"
#include "InputManager.h"

size_t COMTransform::s_hashCode = typeid(COMTransform).hash_code();
size_t COMMaterial::s_hashCode = typeid(COMMaterial).hash_code();
size_t COMSkinnedMesh::s_hashCode = typeid(COMSkinnedMesh).hash_code();
size_t COMDX12SkinnedMeshRenderer::s_hashCode = typeid(COMDX12SkinnedMeshRenderer).hash_code();
size_t COMUIRenderer::s_hashCode = typeid(COMUIRenderer).hash_code();
size_t COMUITransform::s_hashCode = typeid(COMUITransform).hash_code();
size_t COMFontRenderer::s_hashCode = typeid(COMFontRenderer).hash_code();

unsigned int CGHRenderer::s_currRendererInstancedNum = 0;
std::vector<unsigned int> CGHRenderer::s_renderIDPool;
std::unordered_map<unsigned int, std::vector<CGHRenderer::MouseAction>> CGHRenderer::s_mouseActions;

COMTransform::COMTransform(CGHNode* node)
{
}

void COMTransform::Update(CGHNode* node, unsigned int, float delta)
{
	DirectX::XMMATRIX rotateMat = DirectX::XMMatrixRotationQuaternion(DirectX::XMLoadFloat4(&m_queternion));
	DirectX::XMMATRIX scaleMat = DirectX::XMMatrixScaling(m_scale.x, m_scale.y, m_scale.z);
	DirectX::XMMATRIX transMat = DirectX::XMMatrixTranslation(m_pos.x, m_pos.y, m_pos.z);
	DirectX::XMMATRIX ortMat = DirectX::XMLoadFloat4x4(&node->m_srt);

	CGHNode* parentNode = node->GetParent();
	if (parentNode != nullptr)
	{
		DirectX::XMStoreFloat4x4(&node->m_srt, ortMat * scaleMat * rotateMat * transMat * DirectX::XMLoadFloat4x4(&parentNode->m_srt));
	}
	else
	{
		DirectX::XMStoreFloat4x4(&node->m_srt, ortMat * scaleMat * rotateMat * transMat);
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
		std::vector<CGHNode*> childNodeStack;
		childNodeStack.reserve(512);
		node->GetChildNodes(&childNodeStack);

		for (auto iter : childNodeStack)
		{
			m_currNodeTree.insert({ iter->GetName(), iter });
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
	m_currMaterialIndex = DX12GraphicResourceManager::s_insatance.CreateData<CGHMaterial>();

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
	DX12GraphicResourceManager::s_insatance.ReleaseData<CGHMaterial>(m_currMaterialIndex);
	m_currMaterialIndex = 0;

	if (m_textureBuffer)
	{
		delete m_textureBuffer;
	}
}

void COMMaterial::RateUpdate(CGHNode* node, unsigned int currFrame, float delta)
{
	DX12GraphicResourceManager::s_insatance.SetData<CGHMaterial>(m_currMaterialIndex, &m_material);
}

void COMMaterial::SetData(const CGHMaterial* mat)
{
	m_material = *mat;

	//for (int i = 0; i < m_material.numTexture; i++)
	//{
	//	if (m_material.textureInfo->type != aiTextureType_NONE)
	//	{
	//		m_textureBuffer->SetTexture(TextureInfo::GetTexturePath(m_material.textureInfo[i].textureFilePathID).c_str(), i);
	//	}
	//	else
	//	{
	//		m_textureBuffer->SetNullTexture(i);
	//	}

	//}

	//m_textureBuffer->CreateSRVs(m_descHeap->GetCPUDescriptorHandleForHeapStart());
}

void COMMaterial::SetTexture(const TextureInfo* textureInfo, unsigned int index)
{
	if (textureInfo)
	{
		m_textureBuffer->SetTexture(TextureInfo::GetTexturePath(textureInfo->textureFilePathID).c_str(), index);
		std::memcpy(&m_material.textureInfo[index], textureInfo, sizeof(TextureInfo));

		m_textureBuffer->CreateSRVs(m_descHeap->GetCPUDescriptorHandleForHeapStart());

		if (m_material.numTexture <= index)
		{
			m_material.numTexture = index + 1;
		}
	}
	else
	{
		m_material.textureInfo[index].type = aiTextureType_NONE;
	}
}

UINT64 COMMaterial::GetMaterialDataGPU(unsigned int currFrameIndex)
{
	return DX12GraphicResourceManager::s_insatance.GetGpuAddress<CGHMaterial>(m_currMaterialIndex, currFrameIndex);
}

void COMUITransform::Update(CGHNode* node, unsigned int, float delta)
{
	DirectX::XMMATRIX transMat = DirectX::XMMatrixTranslation(m_pos.x, m_pos.y, m_pos.z);

	CGHNode* parentNode = node->GetParent();
	if (parentNode != nullptr)
	{
		DirectX::XMStoreFloat4x4(&node->m_srt, transMat * DirectX::XMLoadFloat4x4(&parentNode->m_srt));
	}
	else
	{
		DirectX::XMStoreFloat4x4(&node->m_srt, transMat);
	}
}

void XM_CALLCONV COMUITransform::SetPos(DirectX::FXMVECTOR pos)
{
	DirectX::XMStoreFloat3(&m_pos, pos);
}

void XM_CALLCONV COMUITransform::SetSize(DirectX::FXMVECTOR size)
{
	DirectX::XMStoreFloat2(&m_size, size);
}

COMUIRenderer::COMUIRenderer(CGHNode* node)
{
	m_color = { 0.0f,0.0f,0.0f, 1.0f };
}

void COMUIRenderer::RateUpdate(CGHNode* node, unsigned int currFrame, float delta)
{
	DirectX::XMFLOAT3 pos = { node->m_srt._41,node->m_srt._42,node->m_srt._43 };
	DirectX::XMFLOAT2 size = node->GetComponent<COMUITransform>()->GetSize();
	if (m_isTextureBackGound)
	{
		GraphicDeviceDX12::GetGraphic()->RenderUI(pos, size, m_spriteSubIndex, m_color.z, GetRenderID(), m_parentRenderID);
	}
	else
	{
		GraphicDeviceDX12::GetGraphic()->RenderUI(pos, size, m_color, GetRenderID(), m_parentRenderID);
	}
}


COMFontRenderer::COMFontRenderer(CGHNode* node)
{
}

void COMFontRenderer::RateUpdate(CGHNode* node, unsigned int currFrame, float delta)
{
	DirectX::XMFLOAT3 pos = { node->m_srt._41,node->m_srt._42,node->m_srt._43 };
	GraphicDeviceDX12::GetGraphic()->RenderString(m_str.c_str(), m_color, pos, m_fontSize, m_rowPitch, m_parentRenderID);
}

void XM_CALLCONV COMFontRenderer::SetColor(DirectX::FXMVECTOR color)
{
	DirectX::XMStoreFloat4(&m_color, color);
}


void COMFontRenderer::SetRenderString(const wchar_t* str, DirectX::FXMVECTOR color, float rowPitch)
{
	m_str = str;
	DirectX::XMStoreFloat4(&m_color, color);
	m_rowPitch = rowPitch;
}

void COMFontRenderer::SetText(const wchar_t* str)
{
	m_str = str;
}

void COMFontRenderer::SetRowPitch(float rowPitch)
{
	m_rowPitch = rowPitch;
}

CGHRenderer::CGHRenderer()
{
	if (s_renderIDPool.size())
	{
		m_renderID = s_renderIDPool.back();
		s_renderIDPool.pop_back();
	}
	else
	{
		assert(s_currRendererInstancedNum < UINT16_MAX);
		m_renderID = s_currRendererInstancedNum++;
	}
}

CGHRenderer::~CGHRenderer()
{
	s_renderIDPool.push_back(m_renderID);
}

void CGHRenderer::ExcuteMouseAction(unsigned int renderID)
{
	if (renderID > 0)
	{
		auto iter = s_mouseActions.find(renderID - 1);

		if (iter != s_mouseActions.end())
		{
			auto& actions = iter->second;
			const auto& mouse = InputManager::GetMouse();
			for (auto& currAction : actions)
			{
				DirectX::Mouse::ButtonStateTracker::ButtonState targetState = DirectX::Mouse::ButtonStateTracker::ButtonState::UP;

				switch (currAction.funcMouseButton)
				{
				case 0:
				{
					targetState = mouse.leftButton;
				}
				break;
				case 1:
				{
					targetState = mouse.middleButton;
				}
				break;
				case 2:
				{
					targetState = mouse.rightButton;
				}
				break;
				case 3:
				{
					targetState = mouse.xButton1;
				}
				break;
				case 4:
				{
					targetState = mouse.xButton2;
				}
				break;
				default:
					assert(false);
					break;
				} 

				if (currAction.funcMouseState == targetState)
				{
					currAction.func(currAction.node);
				}
			}
		}
	}
}

void CGHRenderer::AddFunc(int mousebutton, int mouseState, std::function<void(CGHNode*)> func)
{
	MouseAction action;
	action.node = reinterpret_cast<CGHNode*>(this);
	action.func = func;
	action.funcMouseButton = mousebutton;
	action.funcMouseState = mouseState;

	s_mouseActions[m_renderID].emplace_back(action);
}

void CGHRenderer::SetParentRender(const CGHRenderer* render)
{
	m_parentRenderID = render->GetRenderID();
}

void CGHRenderer::RemoveFuncs()
{
	auto& actions = s_mouseActions[m_renderID];
	actions.clear();
}

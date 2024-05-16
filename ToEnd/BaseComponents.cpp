#include "BaseComponents.h"
#include "CGHBaseClass.h"
#include "GraphicDeivceDX12.h"
#include "DX12GraphicResourceManager.h"
#include "DX12TextureBuffer.h"
#include "Dx12FontManager.h"
#include "InputManager.h"
#include <ppl.h>

size_t COMTransform::s_hashCode = typeid(COMTransform).hash_code();
size_t COMMaterial::s_hashCode = typeid(COMMaterial).hash_code();
size_t COMSkinnedMesh::s_hashCode = typeid(COMSkinnedMesh).hash_code();
size_t COMDX12SkinnedMeshRenderer::s_hashCode = typeid(COMDX12SkinnedMeshRenderer).hash_code();
size_t COMUIRenderer::s_hashCode = typeid(COMUIRenderer).hash_code();
size_t COMUITransform::s_hashCode = typeid(COMUITransform).hash_code();
size_t COMFontRenderer::s_hashCode = typeid(COMFontRenderer).hash_code();

unsigned int COMSkinnedMesh::s_srvUavSize = 0;
unsigned int CGHRenderer::s_currRendererInstancedNum = 0;
std::vector<unsigned int> CGHRenderer::s_renderIDPool;
std::vector<CGHNode*> CGHRenderer::s_hasNode;
std::unordered_map<unsigned int, std::vector<CGHRenderer::MouseAction>> CGHRenderer::s_mouseActions;
std::unordered_map<std::string, CGHRenderer::MouseAction> CGHRenderer::s_globalActions = {};
const PipeLineWorkSet* COMDX12SkinnedMeshRenderer::s_skinnedMeshBoneUpdateCompute = nullptr;

COMTransform::COMTransform(CGHNode* node)
{
}

void COMTransform::Update(CGHNode* node, float delta)
{
	DirectX::XMMATRIX affinMat = DirectX::XMMatrixAffineTransformation(DirectX::XMLoadFloat3(&m_scale), DirectX::XMVectorZero(),
		DirectX::XMLoadFloat4(&m_queternion), DirectX::XMLoadFloat3(&m_pos));

	CGHNode* parentNode = node->GetParent();
	if (parentNode != nullptr)
	{
		DirectX::XMStoreFloat4x4(&node->m_srt, affinMat * DirectX::XMLoadFloat4x4(&parentNode->m_srt));
	}
	else
	{
		DirectX::XMStoreFloat4x4(&node->m_srt, affinMat);
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

	if (s_srvUavSize == 0)
	{
		s_srvUavSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	}
}

COMSkinnedMesh::~COMSkinnedMesh()
{
}

void COMSkinnedMesh::Release(CGHNode* node)
{
	m_boneDatas->Unmap(0, nullptr);
}

void COMSkinnedMesh::RateUpdate(CGHNode* node, float delta)
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
}

void COMSkinnedMesh::Render(CGHNode* node, unsigned int currFrame)
{
	if (m_data)
	{
		DirectX::XMFLOAT4X4* mapped = m_boneDatasCpu[currFrame];
		auto& bones = m_data->bones;

		for (auto& currBone : bones)
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
		//int numBone = bones.size();
		//Concurrency::parallel_for(0, numBone, [mapped, bones, this](int boneIndex)
		//	{
		//		auto& currBone = bones[boneIndex];
		//		auto iter = m_currNodeTree.find(currBone.name);
		//		if (iter != m_currNodeTree.end())
		//		{
		//			DirectX::XMMATRIX mat = DirectX::XMLoadFloat4x4(&currBone.offsetMatrix) * DirectX::XMLoadFloat4x4(&iter->second->m_srt);
		//			DirectX::XMStoreFloat4x4(mapped + boneIndex, DirectX::XMMatrixTranspose(mat));
		//		}
		//		else
		//		{
		//			assert(false);
		//		}
		//	});
	}
}

void COMSkinnedMesh::SetMeshData(const CGHMesh* meshData)
{
	if (m_data == meshData) return;

	m_data = meshData;
	m_VNTBResource = nullptr;

	if (m_data)
	{
		unsigned int numBone = 0;
		auto device = GraphicDeviceDX12::GetDevice();

		assert(m_data->bones.size() < MAXNUMBONE);

		unsigned int numVertex = m_data->numData[MESHDATA_POSITION];
		D3D12_HEAP_PROPERTIES prop = {};
		D3D12_RESOURCE_DESC desc = {};

		prop.Type = D3D12_HEAP_TYPE_DEFAULT;

		desc.Format = DXGI_FORMAT_UNKNOWN;
		desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		desc.Alignment = 0;
		desc.MipLevels = 1;
		desc.Height = 1;
		desc.DepthOrArraySize = 1;
		desc.SampleDesc.Count = 1;
		desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
		desc.Width = sizeof(DirectX::XMFLOAT3) * numVertex * MESHDATA_INDEX;

		ThrowIfFailed(device->CreateCommittedResource(&prop, D3D12_HEAP_FLAG_NONE, &desc,
			D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(m_VNTBResource.GetAddressOf())));

		if (m_srvuavHeap == nullptr)
		{
			D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
			heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
			heapDesc.NumDescriptors = MESHDATA_INDEX + 2 + MESHDATA_INDEX;
			heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

			ThrowIfFailed(device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(m_srvuavHeap.GetAddressOf())));
		}

		auto heapCPU = m_srvuavHeap->GetCPUDescriptorHandleForHeapStart();

		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
		srvDesc.Format = DXGI_FORMAT_UNKNOWN;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Buffer.StructureByteStride = sizeof(DirectX::XMFLOAT3);
		srvDesc.Buffer.NumElements = numVertex;

		for (int i = 0; i < MESHDATA_INDEX; i++)
		{
			device->CreateShaderResourceView(m_data->meshData.Get(), &srvDesc, heapCPU);
			heapCPU.ptr += s_srvUavSize;
			srvDesc.Buffer.FirstElement += numVertex;
		}

		srvDesc.Buffer.StructureByteStride = sizeof(DirectX::XMUINT2);
		srvDesc.Buffer.NumElements = numVertex;
		srvDesc.Buffer.FirstElement = 0;
		device->CreateShaderResourceView(m_data->boneWeightInfos.Get(), &srvDesc, heapCPU);
		heapCPU.ptr += s_srvUavSize;

		srvDesc.Buffer.StructureByteStride = sizeof(DirectX::XMFLOAT2);
		srvDesc.Buffer.NumElements = m_data->numBoneWeights;
		srvDesc.Buffer.FirstElement = 0;
		device->CreateShaderResourceView(m_data->boneWeights.Get(), &srvDesc, heapCPU);
		heapCPU.ptr += s_srvUavSize;

		D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
		uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
		uavDesc.Buffer.CounterOffsetInBytes = 0;
		uavDesc.Buffer.NumElements = numVertex;
		uavDesc.Buffer.StructureByteStride = sizeof(DirectX::XMFLOAT3);

		for (int i = 0; i < MESHDATA_INDEX; i++)
		{
			device->CreateUnorderedAccessView(m_VNTBResource.Get(), nullptr, &uavDesc, heapCPU);
			heapCPU.ptr += s_srvUavSize;
			uavDesc.Buffer.FirstElement += numVertex;
		}

		GraphicDeviceDX12::GetGraphic()->ReservationResourceBarrierBeforeRenderStart(
			CD3DX12_RESOURCE_BARRIER::Transition(m_VNTBResource.Get(), D3D12_RESOURCE_STATE_COMMON,
				D3D12_RESOURCE_STATE_UNORDERED_ACCESS));
	}
}

D3D12_GPU_VIRTUAL_ADDRESS COMSkinnedMesh::GetBoneData(unsigned int currFrame)
{
	auto result = m_boneDatas->GetGPUVirtualAddress();
	result += BONEDATASIZE * currFrame;
	return result;
}

D3D12_GPU_VIRTUAL_ADDRESS COMSkinnedMesh::GetResultMeshData(MESHDATA_TYPE type)
{
	auto result = m_VNTBResource->GetGPUVirtualAddress();
	result += m_data->numData[MESHDATA_POSITION] * sizeof(DirectX::XMFLOAT3) * type;
	return result;
}


void COMSkinnedMesh::NodeTreeDirty()
{
	m_nodeTreeDirty = true;
}

COMDX12SkinnedMeshRenderer::COMDX12SkinnedMeshRenderer(CGHNode* node)
	: CGHRenderer(node)
{
	if (s_skinnedMeshBoneUpdateCompute == nullptr)
	{
		s_skinnedMeshBoneUpdateCompute = GraphicDeviceDX12::GetGraphic()->GetPSOWorkset("SkinnedMeshBoneUpdateCompute");
	}
}

void COMDX12SkinnedMeshRenderer::Render(CGHNode* node, unsigned int)
{
	if (!m_isGroupRenderTarget)
	{
		if (node->GetComponent<COMSkinnedMesh>())
		{
			auto graphc = GraphicDeviceDX12::GetGraphic();
			if (m_currPsow)
			{
				graphc->RenderMesh(s_skinnedMeshBoneUpdateCompute, node);
				graphc->RenderMesh(m_currPsow, node);
			}
		}
	}
}

void COMDX12SkinnedMeshRenderer::SetPSOW(const char* name)
{
	m_currPsow = GraphicDeviceDX12::GetGraphic()->GetPSOWorkset(name);
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

void COMMaterial::RateUpdate(CGHNode* node, float delta)
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

void COMUITransform::Update(CGHNode* node, float delta)
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
	: CGHRenderer(node)
{
	m_color = { 0.0f,0.0f,0.0f, 1.0f };
}

void COMUIRenderer::Render(CGHNode* node, unsigned int)
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
	: CGHRenderer(node)
{
}

void COMFontRenderer::RateUpdate(CGHNode* node, float delta)
{
}

void COMFontRenderer::Render(CGHNode* node, unsigned int)
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

CGHRenderer::CGHRenderer(CGHNode* node)
{
	if (s_renderIDPool.size())
	{
		m_renderID = s_renderIDPool.back();
		s_renderIDPool.pop_back();

		s_hasNode[m_renderID] = node;
	}
	else
	{
		assert(s_currRendererInstancedNum < UINT16_MAX);
		m_renderID = s_currRendererInstancedNum++;
		s_hasNode.emplace_back();
		s_hasNode.back() = node;
	}
}

CGHRenderer::~CGHRenderer()
{
	s_renderIDPool.push_back(m_renderID);
	RemoveFuncs();
}

int CGHRenderer::GetMouseTargetState(int button, const void* mouse)
{
	DirectX::Mouse::ButtonStateTracker::ButtonState targetState = DirectX::Mouse::ButtonStateTracker::ButtonState::UP;

	const DirectX::Mouse::ButtonStateTracker* mouseTracker = reinterpret_cast<const DirectX::Mouse::ButtonStateTracker*>(mouse);

	switch (button)
	{
	case 0:
	{
		targetState = mouseTracker->leftButton;
	}
	break;
	case 1:
	{
		targetState = mouseTracker->middleButton;
	}
	break;
	case 2:
	{
		targetState = mouseTracker->rightButton;
	}
	break;
	case 3:
	{
		targetState = mouseTracker->xButton1;
	}
	break;
	case 4:
	{
		targetState = mouseTracker->xButton2;
	}
	break;
	default:
		assert(false);
		break;
	}

	return targetState;
}


void CGHRenderer::ExcuteMouseAction(unsigned int renderID)
{
	if (renderID > 0)
	{
		unsigned int realRenderID = renderID - 1;
		const auto& mouse = InputManager::GetMouse();

		int targetState = DirectX::Mouse::ButtonStateTracker::ButtonState::UP;

		for (auto& gAction : s_globalActions)
		{
			targetState = GetMouseTargetState(gAction.second.funcMouseButton, &mouse);

			if (gAction.second.funcMouseState == targetState)
			{
				gAction.second.func(s_hasNode[realRenderID]);
			}
		}

		auto iter = s_mouseActions.find(realRenderID);

		if (iter != s_mouseActions.end())
		{
			auto& actions = iter->second;

			for (auto& currAction : actions)
			{
				targetState = GetMouseTargetState(currAction.funcMouseButton, &mouse);

				if (currAction.funcMouseState == targetState)
				{
					currAction.func(currAction.node);
				}
			}
		}
	}
}

void CGHRenderer::AddGlobalAction(const char* name, int mousebutton, int mouseState, std::function<void(CGHNode*)> func)
{
	MouseAction action;
	action.func = func;
	action.funcMouseButton = mousebutton;
	action.funcMouseState = mouseState;

	auto iter = s_globalActions.find(name);

	if (iter == s_globalActions.end())
	{
		s_globalActions[name] = action;
	}
	else
	{
		assert(false);
	}
}

void CGHRenderer::RemoveGlobalAction(const char* name)
{
	s_globalActions.erase(name);
}

void CGHRenderer::AddFunc(int mousebutton, int mouseState, CGHNode* node, std::function<void(CGHNode*)> func)
{
	MouseAction action;
	action.node = node;
	action.func = func;
	action.funcMouseButton = mousebutton;
	action.funcMouseState = mouseState;

	s_mouseActions[m_renderID].emplace_back(action);
}

void CGHRenderer::SetFunc(int mousebutton, int mouseState, CGHNode* node, std::function<void(CGHNode*)> func)
{
	MouseAction action;
	action.node = node;
	action.func = func;
	action.funcMouseButton = mousebutton;
	action.funcMouseState = mouseState;

	s_mouseActions[m_renderID].clear();
	s_mouseActions[m_renderID].emplace_back(action);
}


void CGHRenderer::SetParentRender(const CGHRenderer* render)
{
	m_parentRenderID = render->GetRenderID();
}

void CGHRenderer::RemoveFuncs()
{
	s_mouseActions.erase(m_renderID);
}


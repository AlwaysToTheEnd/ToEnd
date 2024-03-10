#include <assert.h>
#include "DX12SkinnedMeshRenderer.h"
#include "CGHGraphicResource.h"
#include "GraphicDeivceDX12.h"

size_t COMDX12SkinnedMeshRenderer::s_hashCode = typeid(COMTransform).hash_code();

COMDX12SkinnedMeshRenderer::COMDX12SkinnedMeshRenderer()
	: m_meshSet(nullptr)
{

}

void COMDX12SkinnedMeshRenderer::RateUpdate(CGHNode* node, float delta)
{

}

void COMDX12SkinnedMeshRenderer::SetMesh(CGHMeshDataSet* meshDataSet)
{
	assert(m_meshSet->IsSkinnedMesh());

	if (m_meshSet->IsSkinnedMesh())
	{
		m_meshSet = meshDataSet;

		m_nodeBones.resize(m_meshSet->meshs.size());
		ID3D12Device* device = GraphicDeviceDX12::GetDevice();

		for (size_t i = 0; i < m_meshSet->meshs.size(); i++)
		{
			const CGHMesh& currMesh = m_meshSet->meshs[i];
			m_nodeBones[i] = std::make_unique<DX12UploadBuffer<DirectX::XMMATRIX>>(device, currMesh.bones.size(), false);
		}
	}
}

unsigned int COMDX12SkinnedMeshRenderer::Render(ID3D12GraphicsCommandList* cmd, unsigned int rootsigOffset)
{
	if (m_meshSet)
	{
		for (unsigned int i = 0; i < m_meshSet->meshs.size(); i++)
		{
			auto& currMesh = m_meshSet->meshs[i];

			D3D12_VERTEX_BUFFER_VIEW vbView = {};
			vbView.BufferLocation = currMesh.meshData[MESHDATA_POSITION]->GetGPUVirtualAddress();
			vbView.SizeInBytes = sizeof(aiVector3D) * currMesh.numData[MESHDATA_POSITION];
			vbView.StrideInBytes = sizeof(aiVector3D);

			D3D12_INDEX_BUFFER_VIEW ibView = {};
			ibView.BufferLocation = currMesh.meshData[MESHDATA_INDEX]->GetGPUVirtualAddress();
			ibView.Format = DXGI_FORMAT_R32_UINT;
			ibView.SizeInBytes = sizeof(unsigned int) * currMesh.numData[MESHDATA_INDEX];

			cmd->IASetVertexBuffers(0, 1, &vbView);
			cmd->IASetIndexBuffer(&ibView);

			cmd->SetGraphicsRootShaderResourceView(rootsigOffset, currMesh.meshData[MESHDATA_NORMAL]->GetGPUVirtualAddress());
			cmd->SetGraphicsRootShaderResourceView(rootsigOffset + 1, currMesh.meshData[MESHDATA_TAN]->GetGPUVirtualAddress());
			cmd->SetGraphicsRootShaderResourceView(rootsigOffset + 2, currMesh.meshData[MESHDATA_BITAN]->GetGPUVirtualAddress());
			cmd->SetGraphicsRootShaderResourceView(rootsigOffset + 3, currMesh.meshDataUVs[0]->GetGPUVirtualAddress());

			cmd->SetGraphicsRootShaderResourceView(rootsigOffset + 4, currMesh.boneWeightInfos->GetGPUVirtualAddress());
			cmd->SetGraphicsRootShaderResourceView(rootsigOffset + 5, currMesh.boneWeights->GetGPUVirtualAddress());
			cmd->SetGraphicsRootShaderResourceView(rootsigOffset + 6, m_nodeBones[i]->Resource()->GetGPUVirtualAddress());

			cmd->DrawIndexedInstanced(currMesh.numData[MESHDATA_INDEX], 1, 0, 0, 0);
		}
	}

	return rootsigOffset + 7;
}

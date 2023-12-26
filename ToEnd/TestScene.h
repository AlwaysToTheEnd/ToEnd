#pragma once
#include <vector>
#include <DirectXMath.h>
#include <d3d12.h>
#include <wrl.h>
#include "DX12UploadBuffer.h"

struct CGHMeshDataSet;
struct CGHMaterialSet;


class TestScene
{
public:
	TestScene();
	~TestScene();

	void Init();
	void Update(float delta);
	void Render();

private:
	CGHMeshDataSet* m_meshSet;
	CGHMaterialSet* m_materialSet;
	std::vector<DX12UploadBuffer<unsigned int>> m_boneIndices;
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> m_commandList;
	std::vector<Microsoft::WRL::ComPtr<ID3D12CommandAllocator>> m_commadAllocs;
	std::unique_ptr<DX12UploadBuffer<DirectX::XMFLOAT4X4>> m_nodeBones;
	std::vector<DirectX::XMFLOAT4X4> m_updatedBones;
};
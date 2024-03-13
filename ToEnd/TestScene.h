#pragma once
#include <vector>
#include <DirectXMath.h>
#include <d3d12.h>
#include <wrl.h>
#include <unordered_map>
#include "DX12UploadBuffer.h"
#include "CGHGraphicResource.h"	

class DX12TextureBuffer;


class TestScene
{
public:
	TestScene();
	~TestScene();

	void Init();
	void Update(float delta);
	void Render();

private:
	void CreateFontPSO();

private:
	unsigned int m_srvSize = 0;
	CGHMeshDataSet* m_meshSet;
	DX12TextureBuffer* m_textureBuffer;
	DX12NodeData nodeData;
	std::unordered_map<std::string, unsigned int> m_nodeKeys;
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> m_commandList;
	std::vector<Microsoft::WRL::ComPtr<ID3D12CommandAllocator>> m_commadAllocs;
	std::vector<std::unique_ptr<DX12UploadBuffer<DirectX::XMMATRIX>>> m_nodeBones;
	std::unique_ptr<DX12UploadBuffer<CGHMaterial>> m_material;
	std::vector<Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>> m_descHeaps;
};
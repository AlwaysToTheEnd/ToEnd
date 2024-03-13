#pragma once
#include <string>
#include <vector>
#include "assimp\postprocess.h"
#include "CGHGraphicResource.h"

struct ID3D12GraphicsCommandList;
struct aiScene;


class DX12GraphicResourceLoader
{
public:
	void LoadAllData(const std::string& filePath, int removeComponentFlags, ID3D12GraphicsCommandList* cmd,
		CGHMeshDataSet* meshDataOut, std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>>* uploadbuffersOut, DX12NodeData* nodeOut = nullptr);

private:
	void LoadNodeData(const aiScene* scene, DX12NodeData* nodeOut);
	void LoadMaterialData(const aiScene* scene, ID3D12Device* d12Device);
	void LoadAnimationData(const aiScene* scene, CGHMeshDataSet* meshDataOut);
	void LoadMeshData(const aiScene* scene, ID3D12Device* d12Device, ID3D12GraphicsCommandList* cmd, CGHMeshDataSet* meshDataOut,
		std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>>* uploadbuffersOut);

private:
	std::vector<size_t> m_materiIndices;
};


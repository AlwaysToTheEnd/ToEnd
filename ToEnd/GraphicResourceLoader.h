#pragma once
#include <string>
#include "assimp\postprocess.h"
#include "CGHGraphicResource.h"

#define MAXNUM_ADD_MATERIAL 15

struct ID3D12GraphicsCommandList;
class CGHNode;
struct aiScene;

class DX12GraphicResourceLoader
{
public:
	void LoadAllData(const std::string& filePath, int removeComponentFlags, ID3D12GraphicsCommandList* cmd,
		CGHMeshDataSet* meshDataOut, CGHMaterialSet* materialSetOut, std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>>* uploadbuffersOut);

private:
	void LoadNodeData(const aiScene* scene, CGHMeshDataSet* meshDataOut);
	void LoadMaterialData(const aiScene* scene, ID3D12Device* d12Device, CGHMaterialSet* materialSetOut);
	void LoadAnimationData(const aiScene* scene, CGHMeshDataSet* meshDataOut);
	void LoadMeshData(const aiScene* scene, ID3D12Device* d12Device, ID3D12GraphicsCommandList* cmd, CGHMeshDataSet* meshDataOut,
		std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>>* uploadbuffersOut);
};


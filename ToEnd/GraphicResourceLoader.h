#pragma once
#include <string>
#include <vector>
#include "assimp/postprocess.h"
#include "CGHGraphicResource.h"
#include "CGHBaseClass.h"

struct ID3D12GraphicsCommandList;
struct aiScene;

class DX12GraphicResourceLoader
{
public:
	void LoadAllData(const std::string& filePath, int removeComponentFlags, bool triangleCw,
		ID3D12GraphicsCommandList* cmd, std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>>* uploadbuffersOut,
		std::vector<CGHMesh>* meshDataOut, std::vector<CGHMaterial>* materialOut = nullptr,
		std::vector<CGHNode>* nodeOut = nullptr);

	//void LoadAnimation(const std::string& filePath, CGHAnimationGroup* animationsOut);

private:
	void LoadNodeData(const aiScene* scene, std::vector<CGHNode>& nodeOut);
	void LoadMaterialData(const aiScene* scene, ID3D12Device* d12Device);
	void LoadAnimationData(const aiScene* scene, std::vector<CGHMesh>* meshDataOut);
	void LoadMeshData(const aiScene* scene, ID3D12Device* d12Device, ID3D12GraphicsCommandList* cmd, std::vector<CGHMesh>* meshDataOut, 
		std::vector<CGHMaterial>* materialOut, std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>>* uploadbuffersOut);

private:
	std::vector<CGHMaterial> m_currMaterials;
};


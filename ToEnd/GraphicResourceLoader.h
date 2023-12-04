#pragma once
#include <string>
#include "assimp\postprocess.h"
#include "CGHGraphicResource.h"

#define MAXNUM_ADD_MATERIAL 15

struct ID3D12GraphicsCommandList;

class DX12GraphicResourceLoader
{
public:
	void LoadAllData(const std::string& filePath, int removeComponentFlags, ID3D12GraphicsCommandList* cmd,
		CGHMeshDataSet& meshDataOut, CGHMaterialSet& materialSetOut, std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>>& uploadbuffersOUt);

private:

};


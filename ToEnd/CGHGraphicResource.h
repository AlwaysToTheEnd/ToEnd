#pragma once
#include <vector>
#include <string>
#include <unordered_map>
#include <wrl.h>
#include <d3d12.h>

#include "assimp\material.h"
#include "assimp\mesh.h"
#include "DX12UploadBuffer.h"
#include "CGHBaseClass.h"

struct TextureView
{
	aiString texFilePath;
	aiTextureType type = aiTextureType_NONE;
	aiTextureMapping mapping = aiTextureMapping_UV;
	unsigned int uvIndex = 0;
	float blend = 1.0f;
	aiTextureOp textureOp = aiTextureOp_Multiply;
	aiTextureMapMode mapMode[3] = {};
};

#pragma pack(push, 4)
struct CGHMaterial
{
	int shadingModel = 0;
	int twosided = false;
	int wireframe = false;
	int blend = false;
	aiColor3D diffuse;
	float opacity = 1.0f;
	aiColor3D ambient;
	float bumpscaling = 1.0f;
	aiColor3D specular;
	float shininess = 0.0f;
	aiColor3D emissive;
	float refracti = 1.0f;
	aiColor3D transparent;
	float shinpercent = 1.0f;
	aiColor3D reflective;
	float reflectivity = 0.0f;
};
#pragma pack(pop)

struct CGHMaterialSet
{
	std::vector<std::string> names;
	std::vector<CGHMaterial> materials;
	std::vector<std::vector<TextureView>> textureViews;

	std::unique_ptr<DX12UploadBuffer<CGHMaterial>> materialDatas;
	std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> textures;
};

enum MESHDATA_TYPE
{
	MESHDATA_POSITION = 0,
	MESHDATA_INDEX,
	MESHDATA_NORMAL,
	MESHDATA_TAN,
	MESHDATA_BITAN,
	MESHDATA_NUM
};

struct CGHBone
{
	std::string name;
	DirectX::XMFLOAT4X4 offsetMatrix;
};

struct CGHMesh
{
	aiString meshName;
	aiPrimitiveType primitiveType = aiPrimitiveType_POINT;
	int materialIndex = -1;
	int numData[MESHDATA_NUM] = {};
	int numUVData[AI_MAX_NUMBER_OF_TEXTURECOORDS] = {};
	int numUVComponent[AI_MAX_NUMBER_OF_TEXTURECOORDS] = {};

	Microsoft::WRL::ComPtr<ID3D12Resource> meshData[MESHDATA_NUM];
	Microsoft::WRL::ComPtr<ID3D12Resource> meshDataUV[AI_MAX_NUMBER_OF_TEXTURECOORDS];

	std::vector<CGHBone> bones;
	Microsoft::WRL::ComPtr<ID3D12Resource> boneWeightInfos;
	Microsoft::WRL::ComPtr<ID3D12Resource> boneWeights;
};

struct CGHMeshDataSet
{
	std::vector<CGHMesh> meshs;
	std::vector<CGHNode> nodes;
	std::vector<int> nodeParentIndexList;
};
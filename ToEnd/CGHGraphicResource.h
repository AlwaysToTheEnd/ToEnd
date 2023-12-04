#pragma once
#include <vector>
#include <string>
#include <unordered_map>
#include <wrl.h>
#include <d3d12.h>

#include "assimp\material.h"
#include "assimp\mesh.h"
#include "DX12UploadBuffer.h"

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

	std::shared_ptr<DX12UploadBuffer<CGHMaterial>> upMaterials;
	std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> textures;
};

struct BoneImpact
{
	unsigned int offset = 0;
	unsigned int numImpact = 0;
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

struct MeshInfo
{
	aiString meshName;
	aiPrimitiveType primitiveType = aiPrimitiveType_POINT;
	bool hasBone = false;
	int boneSetIndex = -1;
	int materialIndex = -1;
	int numData[MESHDATA_NUM] = {};
	int numUVData[AI_MAX_NUMBER_OF_TEXTURECOORDS] = {};
	int numUVComponent[AI_MAX_NUMBER_OF_TEXTURECOORDS] = {};
	int offsetData[MESHDATA_NUM] = {};
	int offsetUVData[AI_MAX_NUMBER_OF_TEXTURECOORDS] = {};
};

struct CGHBoneSet
{
	std::unordered_map<std::string, int> boneNames;
	std::vector<aiMatrix4x4> offsetMatrix;
	std::vector<BoneImpact> boneImpactInfos;

	Microsoft::WRL::ComPtr<ID3D12Resource> weightList;
	Microsoft::WRL::ComPtr<ID3D12Resource> boneImpacts;
};

struct CGHMeshDataSet
{
	std::vector<MeshInfo> meshInfos;
	std::vector<CGHBoneSet> boneSets;

	Microsoft::WRL::ComPtr<ID3D12Resource> meshData[MESHDATA_NUM];
	Microsoft::WRL::ComPtr<ID3D12Resource> meshDataUV[AI_MAX_NUMBER_OF_TEXTURECOORDS];
};
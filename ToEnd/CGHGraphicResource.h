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

enum CGHRENDER_FLAG
{

};

struct TextureInfo
{
public:
	unsigned int textureFilePathID = 0;
	aiTextureType type = aiTextureType_NONE;
	aiTextureMapping mapping = aiTextureMapping_UV;
	unsigned int uvIndex = 0;
	float blend = 1.0f;
	aiTextureOp textureOp = aiTextureOp_Multiply;
	aiTextureMapMode mapMode[3] = {};
	float pad0[3] = {};

public:
	static size_t GetTextureFilePathID(const char* path);
	static const std::string& GetTexturePath(unsigned int pathID);
	
private:
	static std::unordered_map<std::string, size_t> s_textureIndex;
	static std::vector<std::string> s_textureNames;
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
	unsigned int numTexture = 0;
	TextureInfo textureInfo[7];
	unsigned int renderQueue = 0;
	float pad0[2] = {};
};
#pragma pack(pop)

template <typename T>
struct TimeValue
{
	double time = 0;
	T value;
};

struct AnimBone
{
	std::string name;
	std::vector<TimeValue<DirectX::XMFLOAT3>> posKeys;
	std::vector<TimeValue<DirectX::XMFLOAT3>> scaleKeys;
	std::vector<TimeValue<DirectX::XMFLOAT4>> rotKeys;
	std::vector<TimeValue<DirectX::XMFLOAT4X4>> trafoKeys;
};

struct AnimMesh
{

};

struct AnimMorph
{

};


struct CGHAnimation
{
	double					tickPerSecond = 0.0l;
	double					duration = 0.0;
	std::vector<AnimBone>	animBones;
	std::vector<AnimMesh>	animMeshs;
	std::vector<AnimMorph>	animMorphs;
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
	std::string meshName;
	aiPrimitiveType primitiveType = aiPrimitiveType_POINT;
	int numData[MESHDATA_NUM] = {};
	std::vector<unsigned int> numUVComponent;
	unsigned int materialIndex = 0;

	Microsoft::WRL::ComPtr<ID3D12Resource> meshData[MESHDATA_NUM];
	std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> meshDataUVs;

	std::vector<CGHBone> bones;
	Microsoft::WRL::ComPtr<ID3D12Resource> boneWeightInfos;
	Microsoft::WRL::ComPtr<ID3D12Resource> boneWeights;
};

struct CGHMeshDataSet
{
	std::vector<CGHMesh> meshs;

	bool IsSkinnedMesh()
	{
		return true;
	}
};
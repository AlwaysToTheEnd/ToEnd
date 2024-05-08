#pragma once
#include <vector>
#include <string>
#include <unordered_map>
#include <wrl.h>
#include <d3d12.h>
#include <DirectXMath.h>

#include "assimp/material.h"
#include "assimp/mesh.h"
#include "assimp/anim.h"

enum CGHRENDER_FLAG
{

};

struct TextureInfo
{
	enum ALPHAOP
	{
		ALPHAOP_ONE =0,
		ALPHAOP_MULTIPLY,
		ALPHAOP_INV_MULTIPLY
	};

public:
	unsigned int textureFilePathID = 0;
	unsigned int type = aiTextureType_NONE;
	unsigned int mapping = aiTextureMapping_UV;
	unsigned int uvIndex = 0;
	unsigned int textureOp = aiTextureOp_Add;
	unsigned int mapMode = aiTextureMapMode_Wrap;
	float blend = 1.0f;
	ALPHAOP srcAlphaOp = ALPHAOP_ONE;

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
	static const int CGHMaterialTextureNum = 16;
	int shadingModel = 0;
	int twosided = false;
	int wireframe = false;
	int blend = false;
	aiColor3D diffuse = { 0.9f, 0.4f, 0.4f };
	float opacity = 1.0f;
	aiColor3D ambient;
	float bumpscaling = 1.0f;
	aiColor3D specular;
	float diffuseAlpha = 1.0f;
	aiColor3D emissive;
	float refracti = 1.0f;
	aiColor3D transparent;
	float shinpercent = 1.0f;
	aiColor3D reflective;
	float reflectivity = 0.0f;
	
	float matalness = 0.0f;
	float roughness = 0.4f;

	unsigned int numTexture = 0;
	unsigned int renderQueue = 0;
	TextureInfo textureInfo[CGHMaterialTextureNum];

	void ClearTexture()
	{
		numTexture = 0;

		for (int i = 0; i < CGHMaterialTextureNum; i++)
		{
			textureInfo[i] = TextureInfo();
		}
	}
};
#pragma pack(pop)


enum class ANIMATION_TYPE
{
	NODE_ANIMATION,
	MESH_ANIMATION,
	MORP_ANIMATION,
	NONE_TYPE,
};

struct CGHAnimationGroup
{
	std::vector<ANIMATION_TYPE> types;
	std::vector<aiAnimation> anims;
	const std::unordered_map<std::string, std::string>* rigMapping = nullptr;
};


enum MESHDATA_TYPE
{
	MESHDATA_POSITION = 0,
	MESHDATA_NORMAL,
	MESHDATA_TAN,
	MESHDATA_BITAN,
	MESHDATA_INDEX,
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
	int numBoneWeights = 0;
	std::vector<unsigned int> numUVComponent;

	Microsoft::WRL::ComPtr<ID3D12Resource> meshData;
	Microsoft::WRL::ComPtr<ID3D12Resource> indices;
	std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> meshDataUVs;

	std::vector<CGHBone> bones;
	Microsoft::WRL::ComPtr<ID3D12Resource> boneWeightInfos;
	Microsoft::WRL::ComPtr<ID3D12Resource> boneWeights;

	bool IsSkinnedMesh()
	{
		return bones.size();
	}
};


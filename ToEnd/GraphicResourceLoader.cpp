#include "GraphicResourceLoader.h"
#include <assert.h>
#include <vector>
#include "GraphicDeivceDX12.h"
#include "assimp\scene.h"
#include "assimp\Importer.hpp"
#include "assimp\postprocess.h"

using namespace Assimp;


struct TextureView
{
	aiTextureType type = aiTextureType_NONE;
	aiString texFilePath;
	aiTextureMapping mapping = aiTextureMapping_UV;
	unsigned int uvIndex = 0;
	float blend = 1.0f;
	aiTextureOp textureOp = aiTextureOp_Multiply;
	aiTextureMapMode mapMode[3] = {};
};

struct DX12StructuredVectorData
{
	ComPtr<ID3D12Resource> data;
	std::vector<DXGI_FORMAT> structStack;
};


struct CGHMaterial
{
	aiString name;
	bool twosided = false;
	int shadingModel = 0;
	bool wireframe = false;
	bool blend = false;
	float opacity = 1.0f;
	float bumpscaling = 1.0f;
	float shininess = 0.0f;
	float shinpercent = 1.0f;
	float reflectivity = 0.0f;
	float refracti = 1.0f;
	aiColor3D diffuse;
	aiColor3D ambient;
	aiColor3D specular;
	aiColor3D emissive;
	aiColor3D transparent;
	aiColor3D reflective;

	std::vector<TextureView> textureViews;
};


void DX12GraphicResourceLoader::LoadAllData(const std::string& filePath)
{
	Assimp::Importer importer;
	ID3D12Device* d12Device = GraphicDeviceDX12::GetGraphic()->GetDevice();
	
	importer.SetPropertyInteger(AI_CONFIG_PP_RVC_FLAGS, 
		aiComponent_CAMERAS | aiComponent_TEXTURES | aiComponent_COLORS | aiComponent_LIGHTS);
	const aiScene* scene = importer.ReadFile(filePath,
		aiProcess_Triangulate | aiProcess_RemoveComponent | aiProcess_ConvertToLeftHanded);

	std::vector<CGHMaterial> materials(scene->mNumMaterials);
	{
		const unsigned int numMaterials = scene->mNumMaterials;
		for (unsigned int i = 0; i < numMaterials; i++)
		{
			CGHMaterial& currDumpMat = materials[i];
			const aiMaterial* currMat = scene->mMaterials[i];

			currMat->Get(AI_MATKEY_NAME, currDumpMat.name);
			currMat->Get(AI_MATKEY_TWOSIDED, currDumpMat.twosided);
			currMat->Get(AI_MATKEY_SHADING_MODEL, currDumpMat.shadingModel);
			currMat->Get(AI_MATKEY_ENABLE_WIREFRAME, currDumpMat.wireframe);
			currMat->Get(AI_MATKEY_BLEND_FUNC, currDumpMat.blend);
			currMat->Get(AI_MATKEY_OPACITY, currDumpMat.opacity);
			currMat->Get(AI_MATKEY_BUMPSCALING, currDumpMat.bumpscaling);
			currMat->Get(AI_MATKEY_SHININESS, currDumpMat.shininess);
			currMat->Get(AI_MATKEY_REFLECTIVITY, currDumpMat.reflectivity);
			currMat->Get(AI_MATKEY_SHININESS_STRENGTH, currDumpMat.shinpercent);
			currMat->Get(AI_MATKEY_REFRACTI, currDumpMat.refracti);
			currMat->Get(AI_MATKEY_COLOR_DIFFUSE, currDumpMat.diffuse);
			currMat->Get(AI_MATKEY_COLOR_AMBIENT, currDumpMat.ambient);
			currMat->Get(AI_MATKEY_COLOR_SPECULAR, currDumpMat.specular);
			currMat->Get(AI_MATKEY_COLOR_EMISSIVE, currDumpMat.emissive);
			currMat->Get(AI_MATKEY_COLOR_TRANSPARENT, currDumpMat.transparent);
			currMat->Get(AI_MATKEY_COLOR_REFLECTIVE, currDumpMat.reflective);

			for (int textureType = aiTextureType_NONE; textureType < aiTextureType_UNKNOWN; textureType++)
			{
				TextureView texView;
				texView.type = static_cast<aiTextureType>(i);

				const unsigned int numTextureCurrType = currMat->GetTextureCount(static_cast<aiTextureType>(i));
				for (unsigned int currTextureIndex = 0; currTextureIndex < numTextureCurrType; currTextureIndex++)
				{
					aiReturn result = currMat->GetTexture(texView.type, currTextureIndex,
						&texView.texFilePath, &texView.mapping, &texView.uvIndex, &texView.blend, &texView.textureOp, texView.mapMode);
					assert(result == aiReturn_SUCCESS);
					
					if (result == aiReturn_SUCCESS)
					{
						currDumpMat.textureViews.push_back(texView);
					}
				}

			}
		}
	}

	enum
	{
		MESHDATA_POSITION = 0,
		MESHDATA_INDEX,
		MESHDATA_NORMAL,
		MESHDATA_WEIGHT,
		MESHDATA_UV,
		MESHDATA_NUM
	};

	DX12StructuredVectorData meshData[MESHDATA_NUM];
	{
		struct MeshInfo
		{
			aiString meshName;
			int numData[MESHDATA_NUM] = {};
			int offsetData[MESHDATA_NUM] = {};
		};

		const unsigned int numMeshes = scene->mNumMeshes;
		std::vector<MeshInfo> meshInfos(numMeshes);

		for (int i = 0; i < numMeshes; i++)
		{
			
		}


	}


	importer.FreeScene();
}

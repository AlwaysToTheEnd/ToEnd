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

void DX12GraphicResourceLoader::LoadAllData(const std::string& filePath, ID3D12GraphicsCommandList* cmd)
{
	Assimp::Importer importer;
	ID3D12Device* d12Device = GraphicDeviceDX12::GetGraphic()->GetDevice();

	importer.SetPropertyInteger(AI_CONFIG_PP_RVC_FLAGS,
		aiComponent_CAMERAS | aiComponent_TEXTURES | aiComponent_COLORS | aiComponent_LIGHTS);
	const aiScene* scene = importer.ReadFile(filePath,
		aiProcess_Triangulate | aiProcess_RemoveComponent | aiProcess_SortByPType | aiProcess_ConvertToLeftHanded);
	std::string error = importer.GetErrorString();

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
		MESHDATA_TAN,
		MESHDATA_BITAN,
		MESHDATA_NUM
	};

	struct MeshInfo
	{
		aiString meshName;
		aiPrimitiveType primitiveType = aiPrimitiveType_POINT;
		bool hasBone = false;
		int materialIndex = -1;
		int numData[MESHDATA_NUM] = {};
		int numUVData[AI_MAX_NUMBER_OF_TEXTURECOORDS] = {};
		int numUVComponent[AI_MAX_NUMBER_OF_TEXTURECOORDS] = {};
		int offsetData[MESHDATA_NUM] = {};
		int offsetUVData[AI_MAX_NUMBER_OF_TEXTURECOORDS] = {};
	};

	const unsigned int numMeshes = scene->mNumMeshes;
	std::vector<MeshInfo> meshInfos(numMeshes);

	for (unsigned int i = 0; i < numMeshes; i++)
	{
		aiMesh* currMesh = scene->mMeshes[i];

		meshInfos[i].meshName = currMesh->mName;
		meshInfos[i].materialIndex = currMesh->mMaterialIndex;
		meshInfos[i].hasBone = currMesh->HasBones();

		if (currMesh->HasPositions())
		{
			meshInfos[i].numData[MESHDATA_POSITION] = currMesh->mNumVertices;
		}

		if (currMesh->HasNormals())
		{
			meshInfos[i].numData[MESHDATA_NORMAL] = currMesh->mNumVertices;
		}

		if (currMesh->HasFaces())
		{
			meshInfos[i].primitiveType = static_cast<aiPrimitiveType>(currMesh->mPrimitiveTypes);

			switch (meshInfos[i].primitiveType)
			{
			case aiPrimitiveType_POINT:
				break;
			case aiPrimitiveType_TRIANGLE:
			{
				meshInfos[i].numData[MESHDATA_INDEX] = currMesh->mNumFaces * 3;
			}
			break;
			case aiPrimitiveType_LINE:
			case aiPrimitiveType_POLYGON:
			{
				for (unsigned int j = 0; j < currMesh->mNumFaces; j++)
				{
					meshInfos[i].numData[MESHDATA_INDEX] += currMesh->mFaces[j].mNumIndices;
				}
			}
			break;
			default:
				assert(false);
				break;
			}
		}

		if (currMesh->HasTangentsAndBitangents())
		{
			meshInfos[i].numData[MESHDATA_TAN] = currMesh->mNumVertices;
			meshInfos[i].numData[MESHDATA_BITAN] = currMesh->mNumVertices;
		}

		for (int j = 0; j < AI_MAX_NUMBER_OF_TEXTURECOORDS; j++)
		{
			if (currMesh->HasTextureCoords(j))
			{
				meshInfos[i].numUVData[j] = currMesh->mNumVertices;
				meshInfos[i].numUVComponent[j] = currMesh->mNumUVComponents[j];
			}
		}

		const MeshInfo& prevMeshInfo = meshInfos[(i - 1) >= 0 ? i : 1];
		for (int j = 0; j < MESHDATA_NUM; j++)
		{
			meshInfos[i].offsetData[j] = prevMeshInfo.offsetData[j] + prevMeshInfo.numData[j];
		}

		for (int j = 0; j < AI_MAX_NUMBER_OF_TEXTURECOORDS; j++)
		{
			meshInfos[i].offsetUVData[j] = prevMeshInfo.offsetUVData[j] + prevMeshInfo.numUVData[j];
		}
	}

	DX12StructuredVectorData meshData[MESHDATA_NUM];
	DX12StructuredVectorData meshDataUV[AI_MAX_NUMBER_OF_TEXTURECOORDS];
	ComPtr<ID3D12Resource> meshDataUploadBuffers[MESHDATA_NUM];
	ComPtr<ID3D12Resource> meshDataUVUploadBuffers[AI_MAX_NUMBER_OF_TEXTURECOORDS];
	{
		meshData[MESHDATA_POSITION].structStack.push_back(DXGI_FORMAT_R32G32B32_FLOAT);
		meshData[MESHDATA_INDEX].structStack.push_back(DXGI_FORMAT_R32_UINT);
		meshData[MESHDATA_NORMAL].structStack.push_back(DXGI_FORMAT_R32G32B32_FLOAT);
		meshData[MESHDATA_TAN].structStack.push_back(DXGI_FORMAT_R32G32B32_FLOAT);
		meshData[MESHDATA_BITAN].structStack.push_back(DXGI_FORMAT_R32G32B32_FLOAT);

		int allDataByteSize[MESHDATA_NUM] = {};
		int allUVDataByteSize[AI_MAX_NUMBER_OF_TEXTURECOORDS] = {};

		const MeshInfo& lastMeshInfo = meshInfos.back();

		allDataByteSize[MESHDATA_POSITION] = (lastMeshInfo.numData[MESHDATA_POSITION] + lastMeshInfo.offsetData[MESHDATA_POSITION]) * sizeof(aiVector3D);
		allDataByteSize[MESHDATA_INDEX] = (lastMeshInfo.numData[MESHDATA_INDEX] + lastMeshInfo.offsetData[MESHDATA_INDEX]) * sizeof(unsigned int);
		allDataByteSize[MESHDATA_NORMAL] = (lastMeshInfo.numData[MESHDATA_NORMAL] + lastMeshInfo.offsetData[MESHDATA_NORMAL]) * sizeof(aiVector3D);
		allDataByteSize[MESHDATA_TAN] = (lastMeshInfo.numData[MESHDATA_TAN] + lastMeshInfo.offsetData[MESHDATA_TAN]) * sizeof(aiVector3D);
		allDataByteSize[MESHDATA_BITAN] = (lastMeshInfo.numData[MESHDATA_BITAN] + lastMeshInfo.offsetData[MESHDATA_BITAN]) * sizeof(aiVector3D);

		for (int i = 0; i < AI_MAX_NUMBER_OF_TEXTURECOORDS; i++)
		{
			allUVDataByteSize[i] = (lastMeshInfo.numUVData[i] + lastMeshInfo.offsetUVData[i]) * lastMeshInfo.numUVComponent[i] * sizeof(float);
		}

		D3D12_HEAP_FLAGS heapFlags;
		D3D12_HEAP_PROPERTIES uploadHeapPDesc = {};
		D3D12_HEAP_PROPERTIES defaultHeapPDesc = {};
		D3D12_RESOURCE_DESC resourceDesc = {};

		heapFlags = D3D12_HEAP_FLAG_NONE;

		uploadHeapPDesc.Type = D3D12_HEAP_TYPE_UPLOAD;
		uploadHeapPDesc.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		uploadHeapPDesc.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
		uploadHeapPDesc.CreationNodeMask = 0;
		uploadHeapPDesc.VisibleNodeMask = 0;

		defaultHeapPDesc.Type = D3D12_HEAP_TYPE_DEFAULT;
		defaultHeapPDesc.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		defaultHeapPDesc.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
		defaultHeapPDesc.CreationNodeMask = 0;
		defaultHeapPDesc.VisibleNodeMask = 0;

		resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
		resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		resourceDesc.MipLevels = 1;
		resourceDesc.DepthOrArraySize = 1;
		resourceDesc.Height = 1;
		resourceDesc.Alignment = 0;
		resourceDesc.SampleDesc.Count = 1;
		resourceDesc.SampleDesc.Quality = 0;

		std::vector<D3D12_RESOURCE_BARRIER> defaultBufferBars;
		D3D12_RESOURCE_BARRIER barrier;

		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
		barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
		barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

		for (int i = 0; i < MESHDATA_NUM; i++)
		{
			resourceDesc.Width = allDataByteSize[i];

			if (resourceDesc.Width)
			{
				ThrowIfFailed(d12Device->CreateCommittedResource(&uploadHeapPDesc, heapFlags, &resourceDesc,
					D3D12_RESOURCE_STATE_COPY_SOURCE, nullptr, IID_PPV_ARGS(meshDataUploadBuffers[i].GetAddressOf())));

				ThrowIfFailed(d12Device->CreateCommittedResource(&defaultHeapPDesc, heapFlags, &resourceDesc,
					D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(meshData[i].data.GetAddressOf())));

				barrier.Transition.pResource = meshData[i].data.Get();
				defaultBufferBars.push_back(barrier);
			}
		}

		for (int i = 0; i < AI_MAX_NUMBER_OF_TEXTURECOORDS; i++)
		{
			resourceDesc.Width = allUVDataByteSize[i];

			if (resourceDesc.Width)
			{
				ThrowIfFailed(d12Device->CreateCommittedResource(&uploadHeapPDesc, heapFlags, &resourceDesc,
					D3D12_RESOURCE_STATE_COPY_SOURCE, nullptr, IID_PPV_ARGS(meshDataUVUploadBuffers[i].GetAddressOf())));

				ThrowIfFailed(d12Device->CreateCommittedResource(&defaultHeapPDesc, heapFlags, &resourceDesc,
					D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(meshDataUV[i].data.GetAddressOf())));

				barrier.Transition.pResource = meshDataUV[i].data.Get();
				defaultBufferBars.push_back(barrier);
			}
		}

		unsigned int dataStride[MESHDATA_NUM] = {};
		dataStride[MESHDATA_POSITION] = sizeof(aiVector3D);
		dataStride[MESHDATA_INDEX] = sizeof(unsigned int);
		dataStride[MESHDATA_NORMAL] = sizeof(aiVector3D);
		dataStride[MESHDATA_TAN] = sizeof(aiVector3D);
		dataStride[MESHDATA_BITAN] = sizeof(aiVector3D);

		//cmd->ResourceBarrier(defaultBufferBars.size(), &defaultBufferBars.front());

		for (int i = 0; i < MESHDATA_NUM; i++)
		{
			ID3D12Resource* currData = meshDataUploadBuffers[i].Get();
			if (currData != nullptr)
			{
				D3D12_RANGE range;
				range.Begin = 0;
				range.End = 0;

				BYTE* dataStart = nullptr;
				ThrowIfFailed(currData->Map(0, &range, reinterpret_cast<void**>(&dataStart)));

				for (unsigned int j = 0; j < numMeshes; j++)
				{
					const aiMesh* currMesh = scene->mMeshes[j];
					const MeshInfo& currMeshInfo = meshInfos[j];

					BYTE* currBegin = dataStart + currMeshInfo.offsetData[i];
					int dataSize = currMeshInfo.numData[i] * dataStride[i];

					if (dataSize)
					{
						switch (i)
						{
						case MESHDATA_POSITION:
						{
							std::memcpy(currBegin, currMesh->mVertices, dataSize);
						}
						break;
						case MESHDATA_INDEX:
						{
							for (unsigned int faceIndex = 0; faceIndex < currMesh->mNumFaces; faceIndex++)
							{
								unsigned int faceDataSize = sizeof(unsigned int) * currMesh->mFaces[faceIndex].mNumIndices;
								std::memcpy(currBegin, currMesh->mFaces[faceIndex].mIndices, sizeof(unsigned int) * currMesh->mFaces[faceIndex].mNumIndices);

								currBegin += faceDataSize;
							}
						}
						break;
						case MESHDATA_NORMAL:
						{
							std::memcpy(currBegin, currMesh->mNormals, dataSize);
						}
						break;
						case MESHDATA_TAN:
						{
							std::memcpy(currBegin, currMesh->mTangents, dataSize);
						}
						break;
						case MESHDATA_BITAN:
						{
							std::memcpy(currBegin, currMesh->mBitangents, dataSize);
						}
						break;
						}
					}
				}

				currData->Unmap(0, &range);
			}
		}
	}


	importer.FreeScene();
}

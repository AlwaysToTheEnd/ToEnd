#include <assert.h>
#include "GraphicResourceLoader.h"
#include "GraphicDeivceDX12.h"
#include "assimp\scene.h"
#include "assimp\Importer.hpp"
#include "CGHBaseClass.h"

using namespace Assimp;

struct CopyDataSet
{
	CopyDataSet(size_t size, ComPtr<ID3D12Resource> _dst, ComPtr<ID3D12Resource> _src)
	{
		dataSize = size;
		dst = _dst;
		src = _src;
	}

	size_t dataSize = 0;
	ComPtr<ID3D12Resource> dst;
	ComPtr<ID3D12Resource> src;
};

struct BoneWeightInfo
{
	unsigned int numWeight = 0;
	unsigned int offsetIndex = 0;
};

struct BoneWeight
{
	int boneIndex = 0;
	float weight = 0.0f;
};

void DX12GraphicResourceLoader::LoadAllData(const std::string& filePath, int removeComponentFlags, ID3D12GraphicsCommandList* cmd,
	CGHMeshDataSet* meshDataOut, CGHMaterialSet* materialSetOut, std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>>* uploadbuffersOut)
{
	Assimp::Importer importer;
	ID3D12Device* d12Device = GraphicDeviceDX12::GetGraphic()->GetDevice();

	importer.SetPropertyInteger(AI_CONFIG_PP_RVC_FLAGS, removeComponentFlags);
	const aiScene* scene = importer.ReadFile(filePath,
		aiProcess_Triangulate | aiProcess_RemoveComponent | aiProcess_SortByPType | aiProcess_ConvertToLeftHanded);
	std::string error = importer.GetErrorString();

	assert(scene != nullptr);

	LoadNodeData(scene, meshDataOut);
	LoadMaterialData(scene, d12Device, materialSetOut);
	LoadAnimationData(scene, meshDataOut);
	LoadMeshData(scene, d12Device, cmd, meshDataOut, uploadbuffersOut);

	importer.FreeScene();
}

void DX12GraphicResourceLoader::LoadNodeData(const aiScene* scene, CGHMeshDataSet* meshDataOut)
{
	if (scene->mRootNode)
	{
		std::vector<aiNode*> nodes;
		unsigned int numNodeLastLevel = 1;

		nodes.push_back(scene->mRootNode);
		meshDataOut->nodeParentIndexList.push_back(-1);

		while (numNodeLastLevel)
		{
			unsigned numNodeCurrLevel = 0;
			size_t numNodes = nodes.size();
			for (unsigned int i = 0; i < numNodeLastLevel; i++)
			{
				size_t parentNodeIndex = numNodes - i - 1;
				aiNode* currNode = nodes[parentNodeIndex];
				numNodeCurrLevel += currNode->mNumChildren;

				for (unsigned int j = 0; j < currNode->mNumChildren; j++)
				{
					meshDataOut->nodeParentIndexList.push_back(parentNodeIndex);
					nodes.push_back(currNode->mChildren[j]);
				}
			}

			numNodeLastLevel = numNodeCurrLevel;
		}

		meshDataOut->nodes.resize(nodes.size());
		for (size_t i = 0; i < nodes.size(); i++)
		{
			meshDataOut->nodes[i].SetName(nodes[i]->mName.C_Str());
			std::memcpy(&meshDataOut->nodes[i].m_srt, &nodes[i]->mTransformation, sizeof(aiMatrix4x4));

			CGH::FixEpsilonMatrix(meshDataOut->nodes[i].m_srt, 0.000001f);

			if (meshDataOut->nodeParentIndexList[i] != -1)
			{
				meshDataOut->nodes[i].SetParent(&meshDataOut->nodes[meshDataOut->nodeParentIndexList[i]]);
			}
		}
	}
}

void DX12GraphicResourceLoader::LoadMaterialData(const aiScene* scene, ID3D12Device* d12Device, CGHMaterialSet* materialSetOut)
{
	const unsigned int numMaterials = scene->mNumMaterials;
	materialSetOut->materials.resize(numMaterials);
	materialSetOut->names.resize(numMaterials);
	materialSetOut->textureViews.resize(numMaterials);
	{
		for (unsigned int i = 0; i < numMaterials; i++)
		{
			CGHMaterial& currDumpMat = materialSetOut->materials[i];
			const aiMaterial* currMat = scene->mMaterials[i];

			currMat->Get(AI_MATKEY_NAME, materialSetOut->names[i]);
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
				texView.type = static_cast<aiTextureType>(textureType);

				const unsigned int numTextureCurrType = currMat->GetTextureCount(static_cast<aiTextureType>(textureType));
				for (unsigned int currTextureIndex = 0; currTextureIndex < numTextureCurrType; currTextureIndex++)
				{
					aiReturn result = currMat->GetTexture(texView.type, currTextureIndex,
						&texView.texFilePath, &texView.mapping, &texView.uvIndex, &texView.blend, &texView.textureOp, texView.mapMode);
					assert(result == aiReturn_SUCCESS);

					if (result == aiReturn_SUCCESS)
					{
						materialSetOut->textureViews[i].push_back(texView);
					}
				}
			}

			currDumpMat.numTexture = materialSetOut->textureViews[i].size();
		}
	}

	materialSetOut->materialDatas = std::make_unique<DX12UploadBuffer<CGHMaterial>>(d12Device, numMaterials + MAXNUM_ADD_MATERIAL, true);
	for (unsigned int i = 0; i < numMaterials; i++)
	{
		materialSetOut->materialDatas->CopyData(i, materialSetOut->materials[i]);
	}
}

void DX12GraphicResourceLoader::LoadAnimationData(const aiScene* scene, CGHMeshDataSet* meshDataOut)
{
	for (unsigned int i = 0; i < scene->mNumAnimations; i++)
	{
		assert(scene->mAnimations[i]->mNumMeshChannels == 0);
		assert(scene->mAnimations[i]->mNumMorphMeshChannels == 0);


	}
}

void DX12GraphicResourceLoader::LoadMeshData(const aiScene* scene, ID3D12Device* d12Device, ID3D12GraphicsCommandList* cmd,
	CGHMeshDataSet* meshDataOut, std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>>* uploadbuffersOut)
{
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

	D3D12_RESOURCE_BARRIER barrier;

	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

	unsigned int dataStride[MESHDATA_NUM] = {};
	dataStride[MESHDATA_POSITION] = sizeof(aiVector3D);
	dataStride[MESHDATA_INDEX] = sizeof(unsigned int);
	dataStride[MESHDATA_NORMAL] = sizeof(aiVector3D);
	dataStride[MESHDATA_TAN] = sizeof(aiVector3D);
	dataStride[MESHDATA_BITAN] = sizeof(aiVector3D);

	std::vector<std::vector<BoneWeight>> vertexWeights;
	std::vector<BoneWeight> sirializedWeights;
	std::vector<BoneWeightInfo> boneWeightInfos;
	std::vector<D3D12_RESOURCE_BARRIER> defaultBufferBars;
	std::vector<CopyDataSet> copyDataset;
	const unsigned int numMeshes = scene->mNumMeshes;
	meshDataOut->meshs.resize(numMeshes);
	for (unsigned int i = 0; i < numMeshes; i++)
	{
		aiMesh* currMesh = scene->mMeshes[i];
		CGHMesh& targetMesh = meshDataOut->meshs[i];

		targetMesh.meshName = currMesh->mName;
		targetMesh.materialIndex = currMesh->mMaterialIndex;

		if (currMesh->HasPositions())
		{
			targetMesh.numData[MESHDATA_POSITION] = currMesh->mNumVertices;
		}

		if (currMesh->HasNormals())
		{
			targetMesh.numData[MESHDATA_NORMAL] = currMesh->mNumVertices;
		}

		if (currMesh->HasFaces())
		{
			targetMesh.primitiveType = static_cast<aiPrimitiveType>(currMesh->mPrimitiveTypes);

			switch (targetMesh.primitiveType)
			{
			case aiPrimitiveType_POINT:
				break;
			case aiPrimitiveType_TRIANGLE:
			{
				targetMesh.numData[MESHDATA_INDEX] = currMesh->mNumFaces * 3;
			}
			break;
			case aiPrimitiveType_LINE:
			case aiPrimitiveType_POLYGON:
			{
				for (unsigned int j = 0; j < currMesh->mNumFaces; j++)
				{
					targetMesh.numData[MESHDATA_INDEX] += currMesh->mFaces[j].mNumIndices;
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
			targetMesh.numData[MESHDATA_TAN] = currMesh->mNumVertices;
			targetMesh.numData[MESHDATA_BITAN] = currMesh->mNumVertices;
		}

		for (int j = 0; j < AI_MAX_NUMBER_OF_TEXTURECOORDS; j++)
		{
			if (currMesh->HasTextureCoords(j))
			{
				targetMesh.numUVData[j] = currMesh->mNumVertices;
				targetMesh.numUVComponent[j] = currMesh->mNumUVComponents[j];
			}
		}

		if (currMesh->HasBones())
		{
			const int numBone = currMesh->mNumBones;
			targetMesh.bones.resize(numBone);

			boneWeightInfos.clear();
			vertexWeights.clear();
			sirializedWeights.clear();
			boneWeightInfos.resize(currMesh->mNumVertices);
			vertexWeights.resize(currMesh->mNumVertices);

			unsigned int numAllWeights = 0;

			for (int j = 0; j < numBone; j++)
			{
				const aiBone* currBone = currMesh->mBones[j];

				targetMesh.bones[j].name = currBone->mName.C_Str();
				std::memcpy(&targetMesh.bones[j].offsetMatrix, &currBone->mOffsetMatrix, sizeof(aiMatrix4x4));
				CGH::FixEpsilonMatrix(targetMesh.bones[j].offsetMatrix, 0.000001f);

				for (unsigned int k = 0; k < currBone->mNumWeights; k++)
				{
					vertexWeights[currBone->mWeights[k].mVertexId].push_back({ j, currBone->mWeights[k].mWeight });
				}

				numAllWeights += currBone->mNumWeights;
			}

			sirializedWeights.reserve(numAllWeights);
			size_t currOffsetWeight = 0;
			for (unsigned int j = 0; j < currMesh->mNumVertices; j++)
			{
				const auto& currVertexWeights = vertexWeights[j];
				unsigned currVertexWeightNum = static_cast<unsigned int>(currVertexWeights.size());
				boneWeightInfos[j].numWeight = currVertexWeightNum;
				boneWeightInfos[j].offsetIndex = currOffsetWeight;

				sirializedWeights.insert(sirializedWeights.end(), currVertexWeights.begin(), currVertexWeights.end());

				currOffsetWeight += currVertexWeightNum;
			}

			resourceDesc.Width = boneWeightInfos.size() * sizeof(BoneWeightInfo);
			{
				ComPtr<ID3D12Resource> uploadBuffer;

				ThrowIfFailed(d12Device->CreateCommittedResource(&defaultHeapPDesc, heapFlags, &resourceDesc,
					D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(targetMesh.boneWeightInfos.GetAddressOf())));

				ThrowIfFailed(d12Device->CreateCommittedResource(&uploadHeapPDesc, heapFlags, &resourceDesc,
					D3D12_RESOURCE_STATE_COPY_SOURCE, nullptr, IID_PPV_ARGS(uploadBuffer.GetAddressOf())));

				if (uploadBuffer != nullptr)
				{
					D3D12_RANGE range;
					range.Begin = 0;
					range.End = 0;

					BYTE* dataStart = nullptr;
					ThrowIfFailed(uploadBuffer->Map(0, &range, reinterpret_cast<void**>(&dataStart)));

					std::memcpy(dataStart, boneWeightInfos.data(), resourceDesc.Width);

					uploadBuffer->Unmap(0, &range);
				}

				barrier.Transition.pResource = targetMesh.boneWeightInfos.Get();
				defaultBufferBars.push_back(barrier);
				uploadbuffersOut->push_back(uploadBuffer);
				copyDataset.emplace_back(resourceDesc.Width, targetMesh.boneWeightInfos, uploadBuffer);
			}

			resourceDesc.Width = sirializedWeights.size() * sizeof(BoneWeight);
			{
				ComPtr<ID3D12Resource> uploadBuffer;

				ThrowIfFailed(d12Device->CreateCommittedResource(&defaultHeapPDesc, heapFlags, &resourceDesc,
					D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(targetMesh.boneWeights.GetAddressOf())));

				ThrowIfFailed(d12Device->CreateCommittedResource(&uploadHeapPDesc, heapFlags, &resourceDesc,
					D3D12_RESOURCE_STATE_COPY_SOURCE, nullptr, IID_PPV_ARGS(uploadBuffer.GetAddressOf())));

				if (uploadBuffer != nullptr)
				{
					D3D12_RANGE range;
					range.Begin = 0;
					range.End = 0;

					BYTE* dataStart = nullptr;
					ThrowIfFailed(uploadBuffer->Map(0, &range, reinterpret_cast<void**>(&dataStart)));

					std::memcpy(dataStart, sirializedWeights.data(), resourceDesc.Width);

					uploadBuffer->Unmap(0, &range);
				}

				barrier.Transition.pResource = targetMesh.boneWeights.Get();
				defaultBufferBars.push_back(barrier);
				uploadbuffersOut->push_back(uploadBuffer);
				copyDataset.emplace_back(resourceDesc.Width, targetMesh.boneWeights, uploadBuffer);
			}
		}

		for (int j = 0; j < MESHDATA_NUM; j++)
		{
			ComPtr<ID3D12Resource> uploadBuffer;

			resourceDesc.Width = dataStride[j] * targetMesh.numData[j];
			if (resourceDesc.Width)
			{
				ThrowIfFailed(d12Device->CreateCommittedResource(&defaultHeapPDesc, heapFlags, &resourceDesc,
					D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(targetMesh.meshData[j].GetAddressOf())));

				ThrowIfFailed(d12Device->CreateCommittedResource(&uploadHeapPDesc, heapFlags, &resourceDesc,
					D3D12_RESOURCE_STATE_COPY_SOURCE, nullptr, IID_PPV_ARGS(uploadBuffer.GetAddressOf())));

				if (uploadBuffer != nullptr)
				{
					D3D12_RANGE range;
					range.Begin = 0;
					range.End = 0;

					BYTE* dataStart = nullptr;
					ThrowIfFailed(uploadBuffer->Map(0, &range, reinterpret_cast<void**>(&dataStart)));

					switch (j)
					{
					case MESHDATA_POSITION:
					{
						std::memcpy(dataStart, currMesh->mVertices, resourceDesc.Width);
					}
					break;
					case MESHDATA_INDEX:
					{
						for (unsigned int faceIndex = 0; faceIndex < currMesh->mNumFaces; faceIndex++)
						{
							unsigned int faceDataSize = sizeof(unsigned int) * currMesh->mFaces[faceIndex].mNumIndices;
							std::memcpy(dataStart, currMesh->mFaces[faceIndex].mIndices, faceDataSize);

							dataStart += faceDataSize;
						}
					}
					break;
					case MESHDATA_NORMAL:
					{
						std::memcpy(dataStart, currMesh->mNormals, resourceDesc.Width);
					}
					break;
					case MESHDATA_TAN:
					{
						std::memcpy(dataStart, currMesh->mTangents, resourceDesc.Width);
					}
					break;
					case MESHDATA_BITAN:
					{
						std::memcpy(dataStart, currMesh->mBitangents, resourceDesc.Width);
					}
					break;
					}

					uploadBuffer->Unmap(0, &range);
				}

				barrier.Transition.pResource = targetMesh.meshData[j].Get();
				defaultBufferBars.push_back(barrier);
				uploadbuffersOut->push_back(uploadBuffer);
				copyDataset.emplace_back(resourceDesc.Width, targetMesh.meshData[j], uploadBuffer);
			}
		}

		targetMesh.isUVSingleChannel = currMesh->GetNumUVChannels() == 1;

		if (targetMesh.isUVSingleChannel)
		{
			for (int j = 0; j < AI_MAX_NUMBER_OF_TEXTURECOORDS; j++)
			{
				ComPtr<ID3D12Resource> uploadBuffer;

				resourceDesc.Width = targetMesh.numUVData[j] * sizeof(aiVector3D);
				if (resourceDesc.Width)
				{
					ThrowIfFailed(d12Device->CreateCommittedResource(&defaultHeapPDesc, heapFlags, &resourceDesc,
						D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(targetMesh.meshDataUV[j].GetAddressOf())));

					ThrowIfFailed(d12Device->CreateCommittedResource(&uploadHeapPDesc, heapFlags, &resourceDesc,
						D3D12_RESOURCE_STATE_COPY_SOURCE, nullptr, IID_PPV_ARGS(uploadBuffer.GetAddressOf())));


					if (uploadBuffer != nullptr)
					{
						D3D12_RANGE range;
						range.Begin = 0;
						range.End = 0;

						BYTE* dataStart = nullptr;
						ThrowIfFailed(uploadBuffer->Map(0, &range, reinterpret_cast<void**>(&dataStart)));

						std::memcpy(dataStart, currMesh->mTextureCoords[j], resourceDesc.Width);

						uploadBuffer->Unmap(0, &range);
					}
					
					barrier.Transition.pResource = targetMesh.meshDataUV[j].Get();
					defaultBufferBars.push_back(barrier);
					uploadbuffersOut->push_back(uploadBuffer);
					copyDataset.emplace_back(resourceDesc.Width, targetMesh.meshDataUV[j], uploadBuffer);

					targetMesh.uvSingleChannelIndex = j;
					break;
				}
			}
		}
		else
		{
			struct UVInfo
			{
				int ChannelIndex[8] = { -1, -1, -1, -1,
										-1, -1, -1, -1 };
			};

			unsigned int numChannel = currMesh->GetNumUVChannels();
			std::vector<UVInfo> uvInfos(currMesh->mNumVertices);
			std::vector<aiVector3D> currUVDatas;
			currUVDatas.reserve(currMesh->mNumVertices);

			for (int j = 0; j < AI_MAX_NUMBER_OF_TEXTURECOORDS; j++)
			{
				ComPtr<ID3D12Resource> uploadBuffer;

				unsigned int currUVChannelComponentNum = targetMesh.numUVComponent[j];
				currUVDatas.clear();
				for (unsigned int k = 0; k < targetMesh.numUVData[j]; k++)
				{
					const aiVector3D* currUV = currMesh->mTextureCoords[j] + k;

					bool isUsedUV = true;
					const ai_real* currUVCom = &currUV->x;

					for (unsigned int uvComponentIndex = 0; uvComponentIndex < currUVChannelComponentNum; uvComponentIndex++)
					{
						if (std::abs(*currUVCom) > 5.0f)
						{
							isUsedUV = false;
							break;
						}

						currUVCom++;
					}

					if (isUsedUV)
					{
						uvInfos[k].ChannelIndex[j] = currUVDatas.size();
						currUVDatas.push_back(*currUV);
					}
				}

				targetMesh.numUVData[j] = currUVDatas.size();

				resourceDesc.Width = targetMesh.numUVData[j] * sizeof(aiVector3D);
				if (resourceDesc.Width)
				{
					ThrowIfFailed(d12Device->CreateCommittedResource(&defaultHeapPDesc, heapFlags, &resourceDesc,
						D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(targetMesh.meshDataUV[j].GetAddressOf())));

					ThrowIfFailed(d12Device->CreateCommittedResource(&uploadHeapPDesc, heapFlags, &resourceDesc,
						D3D12_RESOURCE_STATE_COPY_SOURCE, nullptr, IID_PPV_ARGS(uploadBuffer.GetAddressOf())));

					if (uploadBuffer != nullptr)
					{
						D3D12_RANGE range;
						range.Begin = 0;
						range.End = 0;

						BYTE* dataStart = nullptr;
						ThrowIfFailed(uploadBuffer->Map(0, &range, reinterpret_cast<void**>(&dataStart)));

						std::memcpy(dataStart, currUVDatas.data(), resourceDesc.Width);

						uploadBuffer->Unmap(0, &range);
					}

					barrier.Transition.pResource = targetMesh.meshDataUV[j].Get();
					defaultBufferBars.push_back(barrier);
					uploadbuffersOut->push_back(uploadBuffer);
					copyDataset.emplace_back(resourceDesc.Width, targetMesh.meshDataUV[j], uploadBuffer);
				}
			}

			{
				ComPtr<ID3D12Resource> uploadBuffer;

				resourceDesc.Width = uvInfos.size() * sizeof(UVInfo);
				if (resourceDesc.Width)
				{
					ThrowIfFailed(d12Device->CreateCommittedResource(&defaultHeapPDesc, heapFlags, &resourceDesc,
						D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(targetMesh.UVdataInfos.GetAddressOf())));

					ThrowIfFailed(d12Device->CreateCommittedResource(&uploadHeapPDesc, heapFlags, &resourceDesc,
						D3D12_RESOURCE_STATE_COPY_SOURCE, nullptr, IID_PPV_ARGS(uploadBuffer.GetAddressOf())));

					if (uploadBuffer != nullptr)
					{
						D3D12_RANGE range;
						range.Begin = 0;
						range.End = 0;

						BYTE* dataStart = nullptr;
						ThrowIfFailed(uploadBuffer->Map(0, &range, reinterpret_cast<void**>(&dataStart)));

						std::memcpy(dataStart, uvInfos.data(), resourceDesc.Width);

						uploadBuffer->Unmap(0, &range);
					}

					barrier.Transition.pResource = targetMesh.UVdataInfos.Get();
					defaultBufferBars.push_back(barrier);
					uploadbuffersOut->push_back(uploadBuffer);
					copyDataset.emplace_back(resourceDesc.Width, targetMesh.UVdataInfos, uploadBuffer);
				}
			}
		}
	}

	cmd->ResourceBarrier(defaultBufferBars.size(), &defaultBufferBars.front());

	for (auto& iter : copyDataset)
	{
		cmd->CopyBufferRegion(iter.dst.Get(), 0, iter.src.Get(), 0, iter.dataSize);
	}

	for (auto& iter : defaultBufferBars)
	{
		iter.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
		iter.Transition.StateAfter = D3D12_RESOURCE_STATE_GENERIC_READ;
	}

	cmd->ResourceBarrier(defaultBufferBars.size(), &defaultBufferBars.front());
}

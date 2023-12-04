#include <assert.h>
#include "GraphicResourceLoader.h"
#include "GraphicDeivceDX12.h"
#include "assimp\scene.h"
#include "assimp\Importer.hpp"

using namespace Assimp;

void DX12GraphicResourceLoader::LoadAllData(const std::string& filePath, int removeComponentFlags, ID3D12GraphicsCommandList* cmd,
	CGHMeshDataSet& meshDataOut, CGHMaterialSet& materialSetOut, std::vector<ComPtr<ID3D12Resource>>& uploadbuffersOut)
{
	Assimp::Importer importer;
	ID3D12Device* d12Device = GraphicDeviceDX12::GetGraphic()->GetDevice();

	importer.SetPropertyInteger(AI_CONFIG_PP_RVC_FLAGS, removeComponentFlags);
	const aiScene* scene = importer.ReadFile(filePath,
		aiProcess_Triangulate | aiProcess_RemoveComponent | aiProcess_SortByPType | aiProcess_ConvertToLeftHanded);
	std::string error = importer.GetErrorString();

	assert(scene != nullptr);

	const unsigned int numMaterials = scene->mNumMaterials;
	materialSetOut.materials.resize(numMaterials);
	materialSetOut.names.resize(numMaterials);
	materialSetOut.textureViews.resize(numMaterials);
	{
		for (unsigned int i = 0; i < numMaterials; i++)
		{
			CGHMaterial& currDumpMat = materialSetOut.materials[i];
			const aiMaterial* currMat = scene->mMaterials[i];

			currMat->Get(AI_MATKEY_NAME, materialSetOut.names[i]);
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
						materialSetOut.textureViews[i].push_back(texView);
					}
				}
			}
		}
	}

	materialSetOut.upMaterials = std::make_shared<DX12UploadBuffer<CGHMaterial>>(d12Device, numMaterials + MAXNUM_ADD_MATERIAL, false);
	for (unsigned int i = 0; i < numMaterials; i++)
	{
		materialSetOut.upMaterials->CopyData(i, materialSetOut.materials[i]);
	}

	const unsigned int numMeshes = scene->mNumMeshes;
	meshDataOut.meshInfos.resize(numMeshes);
	for (unsigned int i = 0; i < numMeshes; i++)
	{
		aiMesh* currMesh = scene->mMeshes[i];

		meshDataOut.meshInfos[i].meshName = currMesh->mName;
		meshDataOut.meshInfos[i].materialIndex = currMesh->mMaterialIndex;

		if (currMesh->HasBones())
		{
			meshDataOut.meshInfos[i].hasBone = true;
			meshDataOut.meshInfos[i].boneSetIndex = meshDataOut.boneSets.size();

			meshDataOut.boneSets.emplace_back();

			const int numBone = currMesh->mNumBones;
			for (int j = 0; j < numBone; j++)
			{
				if (currMesh->mBones[j]->mNumWeights)
				{
					meshDataOut.boneSets.back().boneNames[currMesh->mBones[j]->mName.C_Str()] = j;
					meshDataOut.boneSets.back().offsetMatrix.push_back(currMesh->mBones[j]->mOffsetMatrix);
				}
			}

			if (meshDataOut.boneSets.back().offsetMatrix.size() == 0)
			{
				meshDataOut.boneSets.pop_back();
				meshDataOut.meshInfos[i].hasBone = false;
				meshDataOut.meshInfos[i].boneSetIndex = -1;
			}
		}

		if (currMesh->HasPositions())
		{
			meshDataOut.meshInfos[i].numData[MESHDATA_POSITION] = currMesh->mNumVertices;
		}

		if (currMesh->HasNormals())
		{
			meshDataOut.meshInfos[i].numData[MESHDATA_NORMAL] = currMesh->mNumVertices;
		}

		if (currMesh->HasFaces())
		{
			meshDataOut.meshInfos[i].primitiveType = static_cast<aiPrimitiveType>(currMesh->mPrimitiveTypes);

			switch (meshDataOut.meshInfos[i].primitiveType)
			{
			case aiPrimitiveType_POINT:
				break;
			case aiPrimitiveType_TRIANGLE:
			{
				meshDataOut.meshInfos[i].numData[MESHDATA_INDEX] = currMesh->mNumFaces * 3;
			}
			break;
			case aiPrimitiveType_LINE:
			case aiPrimitiveType_POLYGON:
			{
				for (unsigned int j = 0; j < currMesh->mNumFaces; j++)
				{
					meshDataOut.meshInfos[i].numData[MESHDATA_INDEX] += currMesh->mFaces[j].mNumIndices;
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
			meshDataOut.meshInfos[i].numData[MESHDATA_TAN] = currMesh->mNumVertices;
			meshDataOut.meshInfos[i].numData[MESHDATA_BITAN] = currMesh->mNumVertices;
		}

		for (int j = 0; j < AI_MAX_NUMBER_OF_TEXTURECOORDS; j++)
		{
			if (currMesh->HasTextureCoords(j))
			{
				meshDataOut.meshInfos[i].numUVData[j] = currMesh->mNumVertices;
				meshDataOut.meshInfos[i].numUVComponent[j] = currMesh->mNumUVComponents[j];
			}
		}

		const MeshInfo& prevMeshInfo = meshDataOut.meshInfos[(i - 1) >= 0 ? i : 1];
		for (int j = 0; j < MESHDATA_NUM; j++)
		{
			meshDataOut.meshInfos[i].offsetData[j] = prevMeshInfo.offsetData[j] + prevMeshInfo.numData[j];
		}

		for (int j = 0; j < AI_MAX_NUMBER_OF_TEXTURECOORDS; j++)
		{
			meshDataOut.meshInfos[i].offsetUVData[j] = prevMeshInfo.offsetUVData[j] + prevMeshInfo.numUVData[j];
		}
	}

	std::vector<ComPtr<ID3D12Resource>> meshDataUploadBuffers(MESHDATA_NUM);
	std::vector<ComPtr<ID3D12Resource>> meshDataUVUploadBuffers(AI_MAX_NUMBER_OF_TEXTURECOORDS);
	std::vector<ComPtr<ID3D12Resource>> boneSetWeightUpladBuffers(meshDataOut.boneSets.size());
	std::vector<ComPtr<ID3D12Resource>> boneImpactUpladBuffers(meshDataOut.boneSets.size());
	{
		int allDataByteSize[MESHDATA_NUM] = {};
		int allUVDataByteSize[AI_MAX_NUMBER_OF_TEXTURECOORDS] = {};

		const MeshInfo& lastMeshInfo = meshDataOut.meshInfos.back();

		allDataByteSize[MESHDATA_POSITION] = (lastMeshInfo.numData[MESHDATA_POSITION] + lastMeshInfo.offsetData[MESHDATA_POSITION]) * sizeof(aiVector3D);
		allDataByteSize[MESHDATA_INDEX] = (lastMeshInfo.numData[MESHDATA_INDEX] + lastMeshInfo.offsetData[MESHDATA_INDEX]) * sizeof(unsigned int);
		allDataByteSize[MESHDATA_NORMAL] = (lastMeshInfo.numData[MESHDATA_NORMAL] + lastMeshInfo.offsetData[MESHDATA_NORMAL]) * sizeof(aiVector3D);
		allDataByteSize[MESHDATA_TAN] = (lastMeshInfo.numData[MESHDATA_TAN] + lastMeshInfo.offsetData[MESHDATA_TAN]) * sizeof(aiVector3D);
		allDataByteSize[MESHDATA_BITAN] = (lastMeshInfo.numData[MESHDATA_BITAN] + lastMeshInfo.offsetData[MESHDATA_BITAN]) * sizeof(aiVector3D);

		for (int i = 0; i < AI_MAX_NUMBER_OF_TEXTURECOORDS; i++)
		{
			allUVDataByteSize[i] = (lastMeshInfo.numUVData[i] + lastMeshInfo.offsetUVData[i]) * sizeof(aiVector3D);
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
					D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(meshDataOut.meshData[i].GetAddressOf())));

				barrier.Transition.pResource = meshDataOut.meshData[i].Get();
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
					D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(meshDataOut.meshDataUV[i].GetAddressOf())));

				barrier.Transition.pResource = meshDataOut.meshDataUV[i].Get();
				defaultBufferBars.push_back(barrier);
			}
		}

		unsigned int numMeshInfos = static_cast<unsigned int>(meshDataOut.meshInfos.size());
		for (unsigned int i = 0; i < numMeshInfos; i++)
		{
			const MeshInfo& currMeshInfo = meshDataOut.meshInfos[i];
			const aiMesh* currMesh = scene->mMeshes[i];

			if (currMeshInfo.hasBone)
			{
				CGHBoneSet& currBoneSet = meshDataOut.boneSets[currMeshInfo.boneSetIndex];
				int allNumBoneWeights = 0;

				for (unsigned int boneIndex = 0; boneIndex < currMesh->mNumBones; boneIndex++)
				{
					const aiBone* currBone = currMesh->mBones[boneIndex];
					const int numWeight = currBone->mNumWeights;

					if (numWeight)
					{
						BoneImpact temp;
						temp.numImpact = numWeight;
						temp.offset = allNumBoneWeights;
						currBoneSet.boneImpactInfos.push_back(temp);

						allNumBoneWeights += numWeight;
					}
				}

				resourceDesc.Width = currBoneSet.boneImpactInfos.size() * sizeof(BoneImpact);
				if (resourceDesc.Width)
				{
					ThrowIfFailed(d12Device->CreateCommittedResource(&uploadHeapPDesc, heapFlags, &resourceDesc,
						D3D12_RESOURCE_STATE_COPY_SOURCE, nullptr, IID_PPV_ARGS(boneImpactUpladBuffers[currMeshInfo.boneSetIndex].GetAddressOf())));

					ThrowIfFailed(d12Device->CreateCommittedResource(&defaultHeapPDesc, heapFlags, &resourceDesc,
						D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(currBoneSet.boneImpacts.GetAddressOf())));

					barrier.Transition.pResource = currBoneSet.boneImpacts.Get();
					defaultBufferBars.push_back(barrier);

					D3D12_RANGE range;
					range.Begin = 0;
					range.End = 0;

					BoneImpact* dataStart = nullptr;
					boneImpactUpladBuffers[currMeshInfo.boneSetIndex]->Map(0, &range, reinterpret_cast<void**>(&dataStart));
					if (currBoneSet.boneImpactInfos.size())
					{
						std::memcpy(dataStart, currBoneSet.boneImpactInfos.data(), currBoneSet.boneImpactInfos.size() * sizeof(BoneImpact));

					}
					boneImpactUpladBuffers[currMeshInfo.boneSetIndex]->Unmap(0, &range);
				}

				resourceDesc.Width = allNumBoneWeights * sizeof(aiVertexWeight);
				if (resourceDesc.Width)
				{
					ThrowIfFailed(d12Device->CreateCommittedResource(&uploadHeapPDesc, heapFlags, &resourceDesc,
						D3D12_RESOURCE_STATE_COPY_SOURCE, nullptr, IID_PPV_ARGS(boneSetWeightUpladBuffers[currMeshInfo.boneSetIndex].GetAddressOf())));

					ThrowIfFailed(d12Device->CreateCommittedResource(&defaultHeapPDesc, heapFlags, &resourceDesc,
						D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(currBoneSet.weightList.GetAddressOf())));

					barrier.Transition.pResource = currBoneSet.weightList.Get();
					defaultBufferBars.push_back(barrier);

					D3D12_RANGE range;
					range.Begin = 0;
					range.End = 0;

					aiVertexWeight* dataStart = nullptr;
					boneSetWeightUpladBuffers[currMeshInfo.boneSetIndex]->Map(0, &range, reinterpret_cast<void**>(&dataStart));
					for (unsigned int boneIndex = 0; boneIndex < currMesh->mNumBones; boneIndex++)
					{
						const aiBone* currBone = currMesh->mBones[boneIndex];

						if (currBone->mNumWeights)
						{
							std::memcpy(dataStart, currBone->mWeights, currBone->mNumWeights * sizeof(aiVertexWeight));

							dataStart += currBone->mNumWeights;
						}
					}
					boneSetWeightUpladBuffers[currMeshInfo.boneSetIndex]->Unmap(0, &range);
				}
			}
		}

		unsigned int dataStride[MESHDATA_NUM] = {};
		dataStride[MESHDATA_POSITION] = sizeof(aiVector3D);
		dataStride[MESHDATA_INDEX] = sizeof(unsigned int);
		dataStride[MESHDATA_NORMAL] = sizeof(aiVector3D);
		dataStride[MESHDATA_TAN] = sizeof(aiVector3D);
		dataStride[MESHDATA_BITAN] = sizeof(aiVector3D);

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
					const MeshInfo& currMeshInfo = meshDataOut.meshInfos[j];

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

		for (int i = 0; i < AI_MAX_NUMBER_OF_TEXTURECOORDS; i++)
		{
			ID3D12Resource* currData = meshDataUVUploadBuffers[i].Get();

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
					const MeshInfo& currMeshInfo = meshDataOut.meshInfos[j];

					BYTE* currBegin = dataStart + currMeshInfo.offsetUVData[i] * sizeof(aiVector3D);
					int dataSize = currMeshInfo.numUVData[i] * sizeof(aiVector3D);

					if (dataSize)
					{
						std::memcpy(currBegin, currMesh->mTextureCoords[i], dataSize);
					}
				}

				currData->Unmap(0, &range);
			}
		}

		cmd->ResourceBarrier(defaultBufferBars.size(), &defaultBufferBars.front());

		for (int i = 0; i < MESHDATA_NUM; i++)
		{
			if (meshDataUploadBuffers[i].Get())
			{
				cmd->CopyBufferRegion(meshDataOut.meshData[i].Get(), 0, meshDataUploadBuffers[i].Get(), 0, allDataByteSize[i]);
			}
		}

		for (int i = 0; i < AI_MAX_NUMBER_OF_TEXTURECOORDS; i++)
		{
			if (meshDataUVUploadBuffers[i].Get())
			{
				cmd->CopyBufferRegion(meshDataOut.meshDataUV[i].Get(), 0, meshDataUVUploadBuffers[i].Get(), 0, allUVDataByteSize[i]);
			}
		}

		for (size_t i = 0; i < meshDataOut.boneSets.size(); i++)
		{
			cmd->CopyBufferRegion(meshDataOut.boneSets[i].boneImpacts.Get(), 0, boneImpactUpladBuffers[i].Get(), 0,
				meshDataOut.boneSets[i].boneImpactInfos.size() * sizeof(BoneImpact));
			cmd->CopyBufferRegion(meshDataOut.boneSets[i].weightList.Get(), 0, boneSetWeightUpladBuffers[i].Get(), 0,
				(meshDataOut.boneSets[i].boneImpactInfos.back().offset + meshDataOut.boneSets[i].boneImpactInfos.back().numImpact) * sizeof(aiVertexWeight));
		}
	}

	uploadbuffersOut.insert(uploadbuffersOut.end(), meshDataUploadBuffers.begin(), meshDataUploadBuffers.end());
	uploadbuffersOut.insert(uploadbuffersOut.end(), meshDataUVUploadBuffers.begin(), meshDataUVUploadBuffers.end());
	uploadbuffersOut.insert(uploadbuffersOut.end(), boneSetWeightUpladBuffers.begin(), boneSetWeightUpladBuffers.end());
	uploadbuffersOut.insert(uploadbuffersOut.end(), boneImpactUpladBuffers.begin(), boneImpactUpladBuffers.end());

	importer.FreeScene();
}

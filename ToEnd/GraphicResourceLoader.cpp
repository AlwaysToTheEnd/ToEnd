#include <assert.h>
#include "d3dx12.h"
#include "GraphicResourceLoader.h"
#include "GraphicDeivceDX12.h"
#include "assimp\scene.h"
#include "assimp\Importer.hpp"
#include "CGHBaseClass.h"
#include "DX12GraphicResourceManager.h"

using namespace Assimp;

struct BoneWeightInfo
{
	unsigned int numWeight = 0;
	unsigned int offsetIndex = 0;
};

struct BoneWeight
{
	unsigned int boneIndex = 0;
	float weight = 0.0f;
};

void DX12GraphicResourceLoader::LoadAllData(const std::string& filePath, int removeComponentFlags, ID3D12GraphicsCommandList* cmd,
	CGHMeshDataSet* meshDataOut, std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>>* uploadbuffersOut, DX12NodeData* nodeOut)
{
	Assimp::Importer importer;
	ID3D12Device* d12Device = GraphicDeviceDX12::GetGraphic()->GetDevice();

	importer.SetPropertyInteger(AI_CONFIG_PP_RVC_FLAGS, removeComponentFlags);
	const aiScene* scene = importer.ReadFile(filePath,
		aiProcess_Triangulate | aiProcess_RemoveComponent | aiProcess_SortByPType | aiProcess_ConvertToLeftHanded);
	std::string error = importer.GetErrorString();

	assert(scene != nullptr);

	m_materiIndices.clear();

	if (nodeOut != nullptr)
	{
		LoadNodeData(scene, nodeOut);
	}

	LoadMaterialData(scene, d12Device);
	LoadAnimationData(scene, meshDataOut);
	LoadMeshData(scene, d12Device, cmd, meshDataOut, uploadbuffersOut);

	importer.FreeScene();
}

void DX12GraphicResourceLoader::LoadNodeData(const aiScene* scene, DX12NodeData* nodeOut)
{
	if (scene->mRootNode)
	{
		std::vector<aiNode*> nodes;
		unsigned int numNodeLastLevel = 1;

		nodes.push_back(scene->mRootNode);
		nodeOut->nodeParentIndexList.push_back(-1);

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
					nodeOut->nodeParentIndexList.push_back(parentNodeIndex);
					nodes.push_back(currNode->mChildren[j]);
				}
			}

			numNodeLastLevel = numNodeCurrLevel;
		}

		nodeOut->nodes.resize(nodes.size());
		for (size_t i = 0; i < nodes.size(); i++)
		{
			DirectX::XMMATRIX transMat = {};
			DirectX::XMVECTOR scale;
			DirectX::XMVECTOR rotQuter;
			DirectX::XMVECTOR pos;
			COMTransform* transform = nodeOut->nodes[i].CreateComponent<COMTransform>();

			std::memcpy(&transMat, &nodes[i]->mTransformation, sizeof(aiMatrix4x4));
			DirectX::XMMatrixDecompose(&scale, &rotQuter, &pos, transMat);
			transform->SetPos(pos);
			transform->SetScale(scale);
			transform->SetRotateQuter(rotQuter);

			nodeOut->nodes[i].SetName(nodes[i]->mName.C_Str());

			if (nodeOut->nodeParentIndexList[i] != -1)
			{
				nodeOut->nodes[i].SetParent(&nodeOut->nodes[nodeOut->nodeParentIndexList[i]]);
			}
		}
	}
}

void DX12GraphicResourceLoader::LoadMaterialData(const aiScene* scene, ID3D12Device* d12Device)
{
	const unsigned int numMaterials = scene->mNumMaterials;
	{
		for (unsigned int i = 0; i < numMaterials; i++)
		{
			CGHMaterial currDumpMat;
			aiString matName;
			const aiMaterial* currMat = scene->mMaterials[i];

			currMat->Get(AI_MATKEY_NAME, matName);
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
				TextureInfo texView;
				texView.type = static_cast<aiTextureType>(textureType);

				const unsigned int numTextureCurrType = currMat->GetTextureCount(static_cast<aiTextureType>(textureType));
				for (unsigned int currTextureIndex = 0; currTextureIndex < numTextureCurrType; currTextureIndex++)
				{
					aiString filePath;
					aiReturn result = currMat->GetTexture(texView.type, currTextureIndex,
						&filePath, &texView.mapping, &texView.uvIndex, &texView.blend, &texView.textureOp, texView.mapMode);

					texView.textureFilePathID = TextureInfo::GetTextureFilePathID(filePath.C_Str());

					assert(result == aiReturn_SUCCESS);

					if (result == aiReturn_SUCCESS)
					{
						currDumpMat.textureInfo[currDumpMat.numTexture] = texView;
						currDumpMat.numTexture++;
					}
				}
			}

			m_materiIndices.push_back(DX12GraphicResourceManager::s_insatance.SetData(matName.C_Str(), &currDumpMat));
		}
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

	D3D12_RESOURCE_BARRIER defaultBarrier;

	defaultBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	defaultBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	defaultBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
	defaultBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
	defaultBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

	D3D12_RESOURCE_BARRIER barrier;

	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_GENERIC_READ;
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
	std::vector<unsigned int> indices;
	const unsigned int numMeshes = scene->mNumMeshes;
	const unsigned int srvSize = d12Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	meshDataOut->meshs.resize(numMeshes);
	for (unsigned int i = 0; i < numMeshes; i++)
	{
		aiMesh* currMesh = scene->mMeshes[i];
		CGHMesh& targetMesh = meshDataOut->meshs[i];
		unsigned int numUVChannel = currMesh->GetNumUVChannels();

		targetMesh.meshName = currMesh->mName.C_Str();
		targetMesh.materialIndex = m_materiIndices[currMesh->mMaterialIndex];

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

		targetMesh.numUVComponent.resize(numUVChannel);
		for (unsigned int j = 0; j < numUVChannel; j++)
		{
			targetMesh.numUVComponent[j] = currMesh->mNumUVComponents[j];
		}

		if (currMesh->HasBones())
		{
			const int numBone = currMesh->mNumBones;

			boneWeightInfos.clear();
			vertexWeights.clear();
			sirializedWeights.clear();
			boneWeightInfos.resize(currMesh->mNumVertices);
			vertexWeights.resize(currMesh->mNumVertices);

			unsigned int numAllWeights = 0;

			for (int j = 0; j < numBone; j++)
			{
				const aiBone* currBone = currMesh->mBones[j];
				if (currBone->mNumWeights > 0)
				{
					unsigned int currTargetBoneIndex = targetMesh.bones.size();
					targetMesh.bones.emplace_back();
					targetMesh.bones.back().name = currBone->mName.C_Str();
					std::memcpy(&targetMesh.bones.back().offsetMatrix, &currBone->mOffsetMatrix, sizeof(aiMatrix4x4));
					CGH::FixEpsilonMatrix(targetMesh.bones.back().offsetMatrix, 0.000001f);

					for (unsigned int k = 0; k < currBone->mNumWeights; k++)
					{
						vertexWeights[currBone->mWeights[k].mVertexId].push_back({ currTargetBoneIndex, currBone->mWeights[k].mWeight });
					}

					numAllWeights += currBone->mNumWeights;
				}
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
					D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(uploadBuffer.GetAddressOf())));

				D3D12_SUBRESOURCE_DATA subResourceData = {};
				subResourceData.pData = boneWeightInfos.data();
				subResourceData.RowPitch = resourceDesc.Width;
				subResourceData.SlicePitch = subResourceData.RowPitch;

				defaultBarrier.Transition.pResource = targetMesh.boneWeightInfos.Get();
				cmd->ResourceBarrier(1, &defaultBarrier);
				UpdateSubresources<1>(cmd, targetMesh.boneWeightInfos.Get(), uploadBuffer.Get(), 0, 0, 1, &subResourceData);

				barrier.Transition.pResource = targetMesh.boneWeightInfos.Get();
				defaultBufferBars.push_back(barrier);
				uploadbuffersOut->push_back(uploadBuffer);
			}

			resourceDesc.Width = sirializedWeights.size() * sizeof(BoneWeight);
			{
				ComPtr<ID3D12Resource> uploadBuffer;

				ThrowIfFailed(d12Device->CreateCommittedResource(&defaultHeapPDesc, heapFlags, &resourceDesc,
					D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(targetMesh.boneWeights.GetAddressOf())));

				ThrowIfFailed(d12Device->CreateCommittedResource(&uploadHeapPDesc, heapFlags, &resourceDesc,
					D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(uploadBuffer.GetAddressOf())));

				D3D12_SUBRESOURCE_DATA subResourceData = {};
				subResourceData.pData = sirializedWeights.data();
				subResourceData.RowPitch = resourceDesc.Width;
				subResourceData.SlicePitch = subResourceData.RowPitch;

				defaultBarrier.Transition.pResource = targetMesh.boneWeights.Get();
				cmd->ResourceBarrier(1, &defaultBarrier);
				UpdateSubresources<1>(cmd, targetMesh.boneWeights.Get(), uploadBuffer.Get(), 0, 0, 1, &subResourceData);

				barrier.Transition.pResource = targetMesh.boneWeights.Get();
				defaultBufferBars.push_back(barrier);
				uploadbuffersOut->push_back(uploadBuffer);
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
					D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(uploadBuffer.GetAddressOf())));

				D3D12_SUBRESOURCE_DATA subResourceData = {};
				subResourceData.RowPitch = resourceDesc.Width;
				subResourceData.SlicePitch = subResourceData.RowPitch;

				switch (j)
				{
				case MESHDATA_POSITION:
				{
					subResourceData.pData = currMesh->mVertices;
				}
				break;
				case MESHDATA_INDEX:
				{
					indices.clear();
					indices.reserve(currMesh->mNumFaces * 3);
					for (unsigned int faceIndex = 0; faceIndex < currMesh->mNumFaces; faceIndex++)
					{
						for (unsigned int z = 0; z < currMesh->mFaces[faceIndex].mNumIndices; z++)
						{
							indices.push_back(currMesh->mFaces[faceIndex].mIndices[z]);
						}
					}

					subResourceData.pData = indices.data();
				}
				break;
				case MESHDATA_NORMAL:
				{
					subResourceData.pData = currMesh->mNormals;
				}
				break;
				case MESHDATA_TAN:
				{
					subResourceData.pData = currMesh->mTangents;
				}
				break;
				case MESHDATA_BITAN:
				{
					subResourceData.pData = currMesh->mBitangents;
				}
				break;
				}

				defaultBarrier.Transition.pResource = targetMesh.meshData[j].Get();
				cmd->ResourceBarrier(1, &defaultBarrier);
				UpdateSubresources<1>(cmd, targetMesh.meshData[j].Get(), uploadBuffer.Get(), 0, 0, 1, &subResourceData);

				barrier.Transition.pResource = targetMesh.meshData[j].Get();
				defaultBufferBars.push_back(barrier);
				uploadbuffersOut->push_back(uploadBuffer);
			}
		}

		if (numUVChannel)
		{
			targetMesh.meshDataUVs.resize(numUVChannel);
			for (unsigned int j = 0; j < numUVChannel; j++)
			{
				ComPtr<ID3D12Resource> uploadBuffer;

				resourceDesc.Width = currMesh->mNumVertices * sizeof(aiVector3D);
				if (resourceDesc.Width)
				{
					ThrowIfFailed(d12Device->CreateCommittedResource(&defaultHeapPDesc, heapFlags, &resourceDesc,
						D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(targetMesh.meshDataUVs[j].GetAddressOf())));

					ThrowIfFailed(d12Device->CreateCommittedResource(&uploadHeapPDesc, heapFlags, &resourceDesc,
						D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(uploadBuffer.GetAddressOf())));

					D3D12_SUBRESOURCE_DATA subResourceData = {};
					subResourceData.pData = currMesh->mTextureCoords[j];
					subResourceData.RowPitch = resourceDesc.Width;
					subResourceData.SlicePitch = subResourceData.RowPitch;

					defaultBarrier.Transition.pResource = targetMesh.meshDataUVs[j].Get();
					cmd->ResourceBarrier(1, &defaultBarrier);
					UpdateSubresources<1>(cmd, targetMesh.meshDataUVs[j].Get(), uploadBuffer.Get(), 0, 0, 1, &subResourceData);

					barrier.Transition.pResource = targetMesh.meshDataUVs[j].Get();
					defaultBufferBars.push_back(barrier);
					uploadbuffersOut->push_back(uploadBuffer);
				}
			}

		}
	}

	cmd->ResourceBarrier(defaultBufferBars.size(), defaultBufferBars.data());
}

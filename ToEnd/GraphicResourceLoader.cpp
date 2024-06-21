#include <assert.h>
#include "d3dx12.h"
#include "GraphicResourceLoader.h"
#include "GraphicDeivceDX12.h"
#include "assimp\scene.h"
#include "assimp\Importer.hpp"

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

void DX12GraphicResourceLoader::LoadAllData(const std::string& filePath, int removeComponentFlags, bool triangleCw,
	ID3D12GraphicsCommandList* cmd, std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>>* uploadbuffersOut,
	std::vector<CGHMesh>* meshDataOut, std::vector<CGHMaterial>* materialOut, std::vector<CGHNode>* nodeOut)
{
	Assimp::Importer importer;
	ID3D12Device* d12Device = GraphicDeviceDX12::GetGraphic()->GetDevice();

	int leftHandedConvert = aiProcess_ConvertToLeftHanded | aiProcess_CalcTangentSpace;

	if (!triangleCw)
	{
		leftHandedConvert ^= aiProcess_FlipWindingOrder;
	}

	importer.SetPropertyInteger(AI_CONFIG_PP_RVC_FLAGS, removeComponentFlags);
	const aiScene* scene = importer.ReadFile(filePath,
		aiProcess_Triangulate | aiProcess_RemoveComponent | leftHandedConvert);
	std::string error = importer.GetErrorString();

	assert(scene != nullptr);


	if (nodeOut != nullptr)
	{
		LoadNodeData(scene, *nodeOut);
	}

	m_currMaterials.clear();
	if (materialOut != nullptr)
	{
		LoadMaterialData(scene, d12Device);
	}

	LoadMeshData(scene, d12Device, cmd, meshDataOut, materialOut, uploadbuffersOut);

	importer.FreeScene();
}

void DX12GraphicResourceLoader::LoadAnimation(const std::string& filePath, CGHAnimationGroup* animationsOut)
{
	Assimp::Importer importer;
	ID3D12Device* d12Device = GraphicDeviceDX12::GetGraphic()->GetDevice();

	int leftHandedConvert = aiProcess_ConvertToLeftHanded;

	const aiScene* scene = importer.ReadFile(filePath, leftHandedConvert);
	std::string error = importer.GetErrorString();
	assert(scene != nullptr);

	struct MatSet
	{
		DirectX::XMFLOAT4X4 offsetMat;
		DirectX::XMFLOAT4X4 stackMat;
	};

	auto boneOffsetMatrices = std::make_shared<std::unordered_map<std::string, CGHAnimationGroup::MatSet>>();
	{
		std::vector<aiNode*> nodeStack;
		nodeStack.push_back(scene->mRootNode);
		while (nodeStack.size())
		{
			aiNode* currNode = nodeStack.back();
			nodeStack.pop_back();

			for (unsigned int i = 0; i < currNode->mNumChildren; i++)
			{
				nodeStack.push_back(currNode->mChildren[i]);
			}

			DirectX::XMMATRIX mat = DirectX::XMMATRIX(&currNode->mTransformation.Transpose().a1);
			DirectX::XMMATRIX patrentMat = DirectX::XMMatrixIdentity();

			if(currNode->mParent)
			{
				auto iter = boneOffsetMatrices->find(currNode->mParent->mName.C_Str());

				if (iter != boneOffsetMatrices->end())
				{
					patrentMat = DirectX::XMLoadFloat4x4(&iter->second.stackMat);
				}
				else
				{
					assert(false);
				}
			}

			mat = DirectX::XMMatrixMultiply(mat, patrentMat);
			DirectX::XMMATRIX inverseMat = DirectX::XMMatrixInverse(nullptr, mat);
			DirectX::XMStoreFloat4x4(&(*boneOffsetMatrices)[currNode->mName.C_Str()].stackMat, mat);
			DirectX::XMStoreFloat4x4(&(*boneOffsetMatrices)[currNode->mName.C_Str()].offsetMat, inverseMat);
		}
	}
	animationsOut->boneOffsetMatrices = boneOffsetMatrices;

	if (scene->HasAnimations())
	{
		unsigned int numAnimation = scene->mNumAnimations;
		if (animationsOut)
		{
			for (int i = 0; i < numAnimation; i++)
			{
				animationsOut->anims.push_back(*(scene->mAnimations[i]));
			}

			animationsOut->types.resize(numAnimation);

			for (int aniIndex = 0; aniIndex < numAnimation; aniIndex++)
			{
				auto currAnimation = scene->mAnimations[aniIndex];

				for (int i = 0; i < currAnimation->mNumChannels; i++)
				{
					std::string nodeName = currAnimation->mChannels[i]->mNodeName.C_Str();
					size_t pos = nodeName.find("$AssimpFbx$");

					if (pos != std::string::npos)
					{
						currAnimation->mChannels[i]->mNodeName = nodeName.substr(0, pos-1);
					}
				}

				ANIMATION_TYPE aniType = ANIMATION_TYPE::NONE_TYPE;

				if (currAnimation->mNumChannels > 0)
				{
					animationsOut->types[aniIndex] = ANIMATION_TYPE::NODE_ANIMATION;

					assert(currAnimation->mNumMeshChannels == 0);
					assert(currAnimation->mNumMorphMeshChannels == 0);
				}
				std::vector<aiNodeAnim*> nodeAnims;

				for(int i = 0; i < currAnimation->mNumChannels; i++)
				{
					nodeAnims.push_back(currAnimation->mChannels[i]);
				}

				if (currAnimation->mNumMeshChannels > 0)
				{
					animationsOut->types[aniIndex] = ANIMATION_TYPE::MESH_ANIMATION;

					assert(currAnimation->mNumChannels == 0);
					assert(currAnimation->mNumMorphMeshChannels == 0);
				}

				if (currAnimation->mNumMorphMeshChannels > 0)
				{
					animationsOut->types[aniIndex] = ANIMATION_TYPE::MORP_ANIMATION;

					assert(currAnimation->mNumChannels == 0);
					assert(currAnimation->mNumMeshChannels == 0);
				}

				currAnimation->mNumChannels = 0;
				currAnimation->mNumMeshChannels = 0;
				currAnimation->mNumMorphMeshChannels = 0;

				currAnimation->mChannels = nullptr;
				currAnimation->mMeshChannels = nullptr;
				currAnimation->mMorphMeshChannels = nullptr;
			}
		}
	}

	importer.FreeScene();
}

void DX12GraphicResourceLoader::LoadNodeData(const aiScene* scene, std::vector<CGHNode>& nodeOut)
{
	if (scene->mRootNode)
	{
		std::vector<aiNode*> nodes;
		std::vector<int> nodeParentIndexList;

		nodes.push_back(scene->mRootNode);
		nodeParentIndexList.push_back(-1);

		unsigned int currIndex = 0;
		while (currIndex < nodes.size())
		{
			auto currNode = nodes[currIndex];
			for (unsigned int i = 0; i < currNode->mNumChildren; i++)
			{
				nodeParentIndexList.push_back(currIndex);
				nodes.push_back(currNode->mChildren[i]);
			}

			currIndex++;
		}

		nodeOut.resize(nodes.size());
		for (size_t i = 0; i < nodes.size(); i++)
		{
			DirectX::XMMATRIX transMat = DirectX::XMMATRIX(&nodes[i]->mTransformation.a1);
			DirectX::XMVECTOR scale;
			DirectX::XMVECTOR rotQuter;
			DirectX::XMVECTOR pos;
			COMTransform* transform = nodeOut[i].CreateComponent<COMTransform>();

			transMat = DirectX::XMMatrixTranspose(transMat);

			DirectX::XMMatrixDecompose(&scale, &rotQuter, &pos, transMat);
			transform->SetPos(pos);
			transform->SetScale(scale);

			transform->SetRotateQuter(rotQuter);

			nodeOut[i].SetName(nodes[i]->mName.C_Str());

			if (nodeParentIndexList[i] != -1)
			{
				nodeOut[i].SetParent(&nodeOut[nodeParentIndexList[i]], true);
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
			m_currMaterials.emplace_back();
			CGHMaterial* currDumpMat = &m_currMaterials.back();
			aiString matName;
			const aiMaterial* currMat = scene->mMaterials[i];

			currMat->Get(AI_MATKEY_NAME, matName);
			currMat->Get(AI_MATKEY_TWOSIDED, currDumpMat->twosided);
			currMat->Get(AI_MATKEY_SHADING_MODEL, currDumpMat->shadingModel);
			currMat->Get(AI_MATKEY_ENABLE_WIREFRAME, currDumpMat->wireframe);
			currMat->Get(AI_MATKEY_BLEND_FUNC, currDumpMat->blend);
			currMat->Get(AI_MATKEY_OPACITY, currDumpMat->opacity);
			currMat->Get(AI_MATKEY_BUMPSCALING, currDumpMat->bumpscaling);
			currMat->Get(AI_MATKEY_REFLECTIVITY, currDumpMat->reflectivity);
			currMat->Get(AI_MATKEY_SHININESS_STRENGTH, currDumpMat->shinpercent);
			currMat->Get(AI_MATKEY_REFRACTI, currDumpMat->refracti);
			currMat->Get(AI_MATKEY_COLOR_DIFFUSE, currDumpMat->diffuse);
			currMat->Get(AI_MATKEY_COLOR_AMBIENT, currDumpMat->ambient);
			currMat->Get(AI_MATKEY_COLOR_SPECULAR, currDumpMat->specular);
			currMat->Get(AI_MATKEY_COLOR_EMISSIVE, currDumpMat->emissive);
			currMat->Get(AI_MATKEY_COLOR_TRANSPARENT, currDumpMat->transparent);
			currMat->Get(AI_MATKEY_COLOR_REFLECTIVE, currDumpMat->reflective);

			for (int textureType = aiTextureType_NONE; textureType < aiTextureType_UNKNOWN; textureType++)
			{
				TextureInfo texView;
				texView.type = textureType;

				const unsigned int numTextureCurrType = currMat->GetTextureCount(static_cast<aiTextureType>(textureType));
				for (unsigned int currTextureIndex = 0; currTextureIndex < numTextureCurrType; currTextureIndex++)
				{
					aiString filePath;
					aiTextureMapping mapping;
					aiTextureOp op;
					aiTextureMapMode mapmode[3] = {};

					aiReturn result = currMat->GetTexture(static_cast<aiTextureType>(textureType), currTextureIndex,
						&filePath, &mapping, &texView.uvIndex, &texView.blend, &op, mapmode);

					texView.mapping = mapping;
					texView.textureOp = op;
					texView.mapMode = mapmode[0];

					texView.textureFilePathID = TextureInfo::GetTextureFilePathID(filePath.C_Str());

					assert(result == aiReturn_SUCCESS);

					if (result == aiReturn_SUCCESS)
					{
						currDumpMat->textureInfo[currDumpMat->numTexture] = texView;
						currDumpMat->numTexture++;
					}
				}
			}
		}
	}
}

void DX12GraphicResourceLoader::LoadMeshData(const aiScene* scene, ID3D12Device* d12Device, ID3D12GraphicsCommandList* cmd, std::vector<CGHMesh>* meshDataOut,
	std::vector<CGHMaterial>* materialOut, std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>>* uploadbuffersOut)
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

	std::vector<std::vector<BoneWeight>> vertexWeights;
	std::vector<BoneWeight> sirializedWeights;
	std::vector<BoneWeightInfo> boneWeightInfos;
	std::vector<D3D12_RESOURCE_BARRIER> defaultBufferBars;
	std::vector<unsigned int> indices;
	std::vector<aiVector3D> meshDatas;
	const unsigned int numMeshes = scene->mNumMeshes;
	const unsigned int srvSize = d12Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	std::vector<CGHMesh>& outMeshs = *meshDataOut;
	outMeshs.resize(numMeshes);
	for (unsigned int i = 0; i < numMeshes; i++)
	{
		aiMesh* currMesh = scene->mMeshes[i];

		CGHMesh& targetMesh = outMeshs[i];
		unsigned int numUVChannel = currMesh->GetNumUVChannels();

		targetMesh.meshName = currMesh->mName.C_Str();

		if (materialOut != nullptr)
		{
			materialOut->push_back(m_currMaterials[currMesh->mMaterialIndex]);
		}

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

			targetMesh.primitiveType = static_cast<aiPrimitiveType>(targetMesh.primitiveType);
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
				aiBone* currBone = currMesh->mBones[j];
				if (currBone->mNumWeights > 0)
				{
					unsigned int currTargetBoneIndex = targetMesh.bones.size();
					targetMesh.bones.emplace_back();
					targetMesh.bones.back().name = currBone->mName.C_Str();
					DirectX::XMMATRIX offsetMat = DirectX::XMMATRIX(&currBone->mOffsetMatrix.a1);
					offsetMat = DirectX::XMMatrixTranspose(offsetMat);
					DirectX::XMStoreFloat4x4(&targetMesh.bones.back().offsetMatrix, offsetMat);

					for (unsigned int k = 0; k < currBone->mNumWeights; k++)
					{
						vertexWeights[currBone->mWeights[k].mVertexId].push_back({ currTargetBoneIndex, currBone->mWeights[k].mWeight });
					}

					numAllWeights += currBone->mNumWeights;
				}
			}

			targetMesh.numBoneWeights = numAllWeights;
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

		{
			unsigned int numVertex = targetMesh.numData[MESHDATA_POSITION];
			ComPtr<ID3D12Resource> uploadBuffer;
			meshDatas.clear();
			meshDatas.resize(numVertex * MESHDATA_INDEX);
			aiVector3D* currData = meshDatas.data();

			std::memcpy(currData, currMesh->mVertices, sizeof(aiVector3D)* numVertex);
			currData += numVertex;
			std::memcpy(currData, currMesh->mNormals, sizeof(aiVector3D)* numVertex);
			currData += numVertex;
			std::memcpy(currData, currMesh->mTangents, sizeof(aiVector3D)* numVertex);
			currData += numVertex;
			std::memcpy(currData, currMesh->mBitangents, sizeof(aiVector3D)* numVertex);
			
			resourceDesc.Width = sizeof(aiVector3D) * meshDatas.size();
			if (resourceDesc.Width)
			{
				ThrowIfFailed(d12Device->CreateCommittedResource(&defaultHeapPDesc, heapFlags, &resourceDesc,
					D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(targetMesh.meshData.GetAddressOf())));

				ThrowIfFailed(d12Device->CreateCommittedResource(&uploadHeapPDesc, heapFlags, &resourceDesc,
					D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(uploadBuffer.GetAddressOf())));

				D3D12_SUBRESOURCE_DATA subResourceData = {};
				subResourceData.RowPitch = resourceDesc.Width;
				subResourceData.SlicePitch = subResourceData.RowPitch;
				subResourceData.pData = meshDatas.data();
				
				defaultBarrier.Transition.pResource = targetMesh.meshData.Get();
				cmd->ResourceBarrier(1, &defaultBarrier);
				UpdateSubresources<1>(cmd, targetMesh.meshData.Get(), uploadBuffer.Get(), 0, 0, 1, &subResourceData);

				barrier.Transition.pResource = targetMesh.meshData.Get();
				defaultBufferBars.push_back(barrier);
				uploadbuffersOut->push_back(uploadBuffer);
			}
		}

		{
			ComPtr<ID3D12Resource> uploadBuffer;
			resourceDesc.Width = sizeof(unsigned int) * targetMesh.numData[MESHDATA_INDEX];
			indices.clear();
			indices.reserve(currMesh->mNumFaces * 3);
			for (unsigned int faceIndex = 0; faceIndex < currMesh->mNumFaces; faceIndex++)
			{
				for (unsigned int z = 0; z < currMesh->mFaces[faceIndex].mNumIndices; z++)
				{
					indices.push_back(currMesh->mFaces[faceIndex].mIndices[z]);
				}
			}

			resourceDesc.Width = sizeof(unsigned int) * indices.size();
			if (resourceDesc.Width)
			{
				ThrowIfFailed(d12Device->CreateCommittedResource(&defaultHeapPDesc, heapFlags, &resourceDesc,
					D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(targetMesh.indices.GetAddressOf())));

				ThrowIfFailed(d12Device->CreateCommittedResource(&uploadHeapPDesc, heapFlags, &resourceDesc,
					D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(uploadBuffer.GetAddressOf())));

				D3D12_SUBRESOURCE_DATA subResourceData = {};
				subResourceData.RowPitch = resourceDesc.Width;
				subResourceData.SlicePitch = subResourceData.RowPitch;
				subResourceData.pData = indices.data();

				defaultBarrier.Transition.pResource = targetMesh.indices.Get();
				cmd->ResourceBarrier(1, &defaultBarrier);
				UpdateSubresources<1>(cmd, targetMesh.indices.Get(), uploadBuffer.Get(), 0, 0, 1, &subResourceData);

				barrier.Transition.pResource = targetMesh.indices.Get();
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

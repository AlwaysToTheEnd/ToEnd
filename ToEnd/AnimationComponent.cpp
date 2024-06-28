#include "AnimationComponent.h"
#include <ppl.h>
#include "CGHBaseClass.h"
#include "../Common/Source/CGHUtil.h"

size_t COMAnimator::s_hashCode = typeid(COMAnimator).hash_code();
using namespace DirectX;

COMAnimator::COMAnimator(CGHNode* node)
{
	node->AddEvent(std::bind(&COMAnimator::NodeTreeDirty, this), CGHNODE_EVENT_FLAG_CHILDTREE_CHANGED);
}

void COMAnimator::Release(CGHNode* ndoe)
{
}

void COMAnimator::Update(CGHNode* node, float delta)
{
	const aiAnimation* currAnimation = nullptr;

	if (m_currState && (currAnimation = m_currState->targetAnimation))
	{
		if (m_nodeTreeDirty)
		{
			m_currNodeTree.clear();
			std::vector<CGHNode*> childNodeStack;
			childNodeStack.reserve(512);
			node->GetChildNodes(&childNodeStack);
			m_currNodeTree.reserve(childNodeStack.size());

			for (auto iter : childNodeStack)
			{
				m_currNodeTree.insert({ iter->GetName(), iter });
			}

			m_nodeTreeDirty = false;
		}

		m_currTime += delta * m_currState->speed;

		double duration = currAnimation->mDuration;
		double tickPerSecond = currAnimation->mTicksPerSecond;
		double currFrame = (m_currTime * tickPerSecond);

		if (currFrame > duration)
		{
			currFrame = std::fmod(currFrame, duration);
			m_currTime = currFrame / tickPerSecond;
		}

		Concurrency::parallel_invoke(
			std::bind(&COMAnimator::NodeAnimation, this, currAnimation, currFrame),
			std::bind(&COMAnimator::MeshAnimation, this, currAnimation, currFrame, node),
			std::bind(&COMAnimator::MorphAnimation, this, currAnimation, currFrame, node));
	}
	else
	{
		m_currTime = 0;
	}
}

void COMAnimator::SetAnimation(unsigned int animationIndex, unsigned int stateIndex)
{
	if (m_currGroup->anims.size() > animationIndex)
	{
		m_states.resize(1);
		m_states.front().targetAnimation = &m_currGroup->anims[animationIndex];
		m_currState = &m_states.front();
	}
}

//void COMAnimator::AnimationRigging(CGHAnimationGroup* group, 
//	const std::unordered_map<std::string, CGHAnimationGroup::NodeData>& meshBone,
//	const std::vector<CGHAnimationGroup::NodeData*>& nodeList)
//{
//	assert(group->rigMapping);
//
//	static std::unordered_map<std::string, DirectX::XMFLOAT4X4> fixOffsetMatsMap;
//
//	auto rigMapping = group->rigMapping;
//
//	if (group->nodeDatas.size())
//	{
//		fixOffsetMatsMap.clear();
//
//		const auto aniBones = group->nodeDatas;
//
//		for (auto& iter : *rigMapping)
//		{
//			auto baseIter = meshBone.find(iter.second);
//			auto aniIter = group->nodeDatas.find(iter.first);
//
//			if (baseIter != meshBone.end() && aniIter != group->nodeDatas.end())
//			{
//				DirectX::XMMATRIX baseMat = DirectX::XMLoadFloat4x4(&baseIter->second.offsetMatrix);
//				DirectX::XMMATRIX aniMat = DirectX::XMLoadFloat4x4(&aniIter->second.offsetMatrix);
//
//				DirectX::XMStoreFloat4x4(&fixOffsetMatsMap[iter.first], DirectX::XMMatrixInverse(nullptr, baseMat) * aniMat);
//			}
//			else
//			{
//				assert(false);
//			}
//		}
//
//		std::vector<aiNodeAnim*> channels;
//		std::vector<DirectX::XMMATRIX> fixOffsetMats;
//
//		for (auto& currAnim : group->anims)
//		{
//			unsigned int currChannelNum = currAnim.mNumChannels;
//			channels.resize(group->nodedataList.size());
//			fixOffsetMats.resize(group->nodedataList.size());
//			currAnim.mDuration;
//			for (int i = 0; i < currChannelNum; i++)
//			{
//				auto currChannel = currAnim.mChannels[i];
//			
//				auto iter = group->nodeDatas.find(currChannel->mNodeName.C_Str());
//				assert(iter != group->nodeDatas.end());
//
//				int index = group->nodeDatas[currChannel->mNodeName.C_Str()].index;
//				channels[index] = currChannel;
//
//				auto fixoffsetIter = fixOffsetMatsMap.find(currChannel->mNodeName.C_Str());
//				fixOffsetMats[index] = DirectX::XMLoadFloat4x4(&fixoffsetIter->second);
//			}
//
//			int animationDuration = currAnim.mDuration;
//			
//			// M1 x M2 X M3 = M4 x b x M2 x M5, b = M4^-1 x M1 x M3 x M5^-1
//		    // M4^-1 x M1 = fixOffsetMat, M3 = stackMats(animation),  M5^-1 = invsMat stackMats(baseMeshAim)
//		
//			std::vector<DirectX::XMMATRIX> stackMatsAnim;
//			std::vector<DirectX::XMMATRIX> stackMatsBase;
//			std::vector<DirectX::XMMATRIX> displacementMats;
//
//			for (int currTick = 0; currTick < animationDuration; currTick++)
//			{
//				stackMatsAnim.resize(group->nodedataList.size(), DirectX::XMMatrixIdentity());
//				stackMatsBase.resize(group->nodedataList.size(), DirectX::XMMatrixIdentity());
//				displacementMats.resize(group->nodedataList.size(), DirectX::XMMatrixIdentity());
//
//				for (int index = 0; index < channels.size(); index++)
//				{
//					auto currChannel = channels[index];
//					auto currNode = group->nodedataList[index];
//
//					DirectX::XMMATRIX stackedParentMatAni = DirectX::XMMatrixIdentity();
//					DirectX::XMMATRIX stackedParentMatBase = DirectX::XMMatrixIdentity();
//					DirectX::XMMATRIX currAnimatMat = DirectX::XMMatrixIdentity();
//
//					if(currNode->parent)
//					{
//						stackedParentMatAni = stackMatsAnim[currNode->parent->index];
//						stackedParentMatBase = stackMatsBase[currNode->parent->index];
//					}
//
//					DirectX::XMVECTOR scale = DirectX::XMVectorSet(1.0f, 1.0f, 1.0f, 0.0f);
//					DirectX::XMVECTOR rotate = DirectX::XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
//					DirectX::XMVECTOR pos = DirectX::XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
//
//					for(int i = 0; i < currChannel->mNumScalingKeys -1; i++)
//					{
//						if (currChannel->mScalingKeys[i].mTime >= currTick && currChannel->mScalingKeys[i + 1].mTime < currTick)
//						{
//						    double spot = (currTick - currChannel->mScalingKeys[i].mTime) / (currChannel->mScalingKeys[i + 1].mTime - currChannel->mScalingKeys[i].mTime);
//							 DirectX::XMVECTOR prevScale = DirectX::XMLoadFloat3(reinterpret_cast<const XMFLOAT3*>(&currChannel->mScalingKeys[i].mValue));
//							 DirectX::XMVECTOR nextScale = DirectX::XMLoadFloat3(reinterpret_cast<const XMFLOAT3*>(&currChannel->mScalingKeys[i + 1].mValue));
//							 scale = DirectX::XMVectorLerp(prevScale, nextScale, spot);
//							break;
//						}
//					}
//
//					for (int i = 0; i < currChannel->mNumRotationKeys - 1; i++)
//					{
//						if (currChannel->mRotationKeys[i].mTime >= currTick && currChannel->mRotationKeys[i + 1].mTime < currTick)
//						{
//							double spot = (currTick - currChannel->mRotationKeys[i].mTime) / (currChannel->mRotationKeys[i + 1].mTime - currChannel->mRotationKeys[i].mTime);
//							const aiQuaternion& prev = currChannel->mRotationKeys[i].mValue;
//							const aiQuaternion& next = currChannel->mRotationKeys[i + 1].mValue;
//							rotate = DirectX::XMQuaternionSlerp(DirectX::XMVectorSet(prev.x, prev.y, prev.z, prev.w),
//								DirectX::XMVectorSet(next.x, next.y, next.z, next.w), spot);
//							break;
//						}
//					}
//
//					for (int i = 0; i < currChannel->mNumPositionKeys - 1; i++)
//					{
//						if (currChannel->mPositionKeys[i].mTime >= currTick && currChannel->mPositionKeys[i + 1].mTime < currTick)
//						{
//							double spot = (currTick - currChannel->mPositionKeys[i].mTime) / (currChannel->mPositionKeys[i + 1].mTime - currChannel->mPositionKeys[i].mTime);
//							pos = DirectX::XMVectorLerp(DirectX::XMLoadFloat3(reinterpret_cast<const XMFLOAT3*>(&currChannel->mPositionKeys[i].mValue)),
//								DirectX::XMLoadFloat3(reinterpret_cast<const XMFLOAT3*>(&currChannel->mPositionKeys[i + 1].mValue)), spot);
//							break;
//						}
//					}
//
//					DirectX::XMMATRIX animMat = DirectX::XMMatrixAffineTransformation(scale, DirectX::XMVectorZero(), rotate, pos);
//					DirectX::XMMATRIX displacementMat = fixOffsetMats[index] * stackedParentMatAni * DirectX::XMMatrixInverse(nullptr, stackedParentMatBase);
//
//					displacementMats[index] = displacementMat;
//					stackMatsAnim[index] = animMat * stackedParentMatAni;
//					stackMatsBase[index] = displacementMat * animMat * stackedParentMatBase;
//				}
//			}
//			
//		}
//	}
//}

void COMAnimator::NodeTreeDirty()
{
	m_nodeTreeDirty = true;
}

void COMAnimator::NodeAnimation(const aiAnimation* anim, double currFrame)
{
	int currChannelNum = anim->mNumChannels;

	std::vector<std::string> nodeNames;
	for (int i = 0; i < currChannelNum; i++)
	{
		nodeNames.push_back(anim->mChannels[i]->mNodeName.C_Str());
	}

	Concurrency::parallel_for(0, currChannelNum, [anim, currFrame, this](int index)
		{
			auto currChannel = anim->mChannels[index];
			CGHNode* currNode = nullptr;
			if (m_currGroup->rigMapping)
			{
				auto rigMappingIter = m_currGroup->rigMapping->find(currChannel->mNodeName.C_Str());

				if (rigMappingIter != m_currGroup->rigMapping->end())
				{
					auto nodeIter = m_currNodeTree.find(rigMappingIter->second.c_str());
					if (nodeIter != m_currNodeTree.end())
					{
						currNode = nodeIter->second;
					}
				}
			}
			else
			{
				auto nodeIter = m_currNodeTree.find(currChannel->mNodeName.C_Str());
				if (nodeIter != m_currNodeTree.end())
				{
					currNode = nodeIter->second;
				}
			}

			if (currNode)
			{
				unsigned int numscale = currChannel->mNumScalingKeys;
				unsigned int numRot = currChannel->mNumRotationKeys;
				unsigned int numPos = currChannel->mNumPositionKeys;

				XMVECTOR xmScale = XMVectorZero();
				XMVECTOR xmRotate = XMVectorZero();
				XMVECTOR xmPos = XMVectorZero();

				for (int i = 0; i < numscale - 1; i++)
				{
					double prevTime = currChannel->mScalingKeys[i].mTime;
					double nextTime = currChannel->mScalingKeys[i + 1].mTime;
					if (prevTime <= currFrame && nextTime > currFrame)
					{
						double spot = (currFrame - prevTime) / (nextTime - prevTime);
						xmScale = XMVectorLerp(XMLoadFloat3(reinterpret_cast<const XMFLOAT3*>(&currChannel->mScalingKeys[i].mValue)),
							XMLoadFloat3(reinterpret_cast<const XMFLOAT3*>(&currChannel->mScalingKeys[i + 1].mValue)), spot);
						break;
					}
				}

				for (int i = 0; i < numRot - 1; i++)
				{
					double prevTime = currChannel->mRotationKeys[i].mTime;
					double nextTime = currChannel->mRotationKeys[i + 1].mTime;
					if (prevTime <= currFrame && nextTime > currFrame)
					{
						const aiQuaternion& prev = currChannel->mRotationKeys[i].mValue;
						const aiQuaternion& next = currChannel->mRotationKeys[i + 1].mValue;
						double spot = (currFrame - prevTime) / (nextTime - prevTime);
						xmRotate = XMQuaternionSlerp(DirectX::XMVectorSet(prev.x, prev.y, prev.z, prev.w),
							DirectX::XMVectorSet(next.x, next.y, next.z, next.w), spot);
						break;
					}
				}

				for (int i = 0; i < numPos - 1; i++)
				{
					double prevTime = currChannel->mPositionKeys[i].mTime;
					double nextTime = currChannel->mPositionKeys[i + 1].mTime;
					if (prevTime <= currFrame && nextTime > currFrame)
					{
						double spot = (currFrame - prevTime) / (nextTime - prevTime);
						xmPos = XMVectorLerp(XMLoadFloat3(reinterpret_cast<const XMFLOAT3*>(&currChannel->mPositionKeys[i].mValue)),
							XMLoadFloat3(reinterpret_cast<const XMFLOAT3*>(&currChannel->mPositionKeys[i + 1].mValue)), spot);
						break;
					}
				}

				auto transform = currNode->GetComponent<COMTransform>();

				if (transform)
				{
					//transform->SetScale(xmScale);
					transform->SetRotateQuter(xmRotate);
					//transform->SetPos(xmPos);
				}
			}
		});
}

void COMAnimator::MeshAnimation(const aiAnimation* anim, double currFrame, CGHNode* node)
{
}

void COMAnimator::MorphAnimation(const aiAnimation* anim, double currFrame, CGHNode* node)
{
}

void COMAnimator::GUIRender_Internal(unsigned int currFrame)
{

}

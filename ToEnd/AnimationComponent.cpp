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
					transform->SetPos(xmPos);
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

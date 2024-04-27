#include "AnimationComponent.h"
#include <ppl.h>
#include "CGHBaseClass.h"

size_t COMAnimator::s_hashCode = typeid(COMAnimator).hash_code();
using namespace DirectX;

COMAnimator::COMAnimator(CGHNode* node)
{
	node->AddEvent(std::bind(&COMAnimator::NodeTreeDirty, this), CGHNODE_EVENT_FLAG_CHILDTREE_CHANGED);
}

void COMAnimator::Release(CGHNode* ndoe)
{
}

void COMAnimator::Update(CGHNode* node, unsigned int, float delta)
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

		if (currAnimation)
		{
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
				std::bind(&COMAnimator::NodeAnimation, this, currAnimation),
				std::bind(&COMAnimator::MeshAnimation, this, currAnimation, node),
				std::bind(&COMAnimator::MorphAnimation, this, currAnimation, node));
		}
	}
	else
	{
		m_currTime = 0;
	}
}

void COMAnimator::SetAnimation(const aiAnimation* anim, unsigned int stateIndex)
{
}

void COMAnimator::NodeTreeDirty()
{
	m_nodeTreeDirty = true;
}

void COMAnimator::NodeAnimation(const aiAnimation* anim)
{
	int currChannelNum = anim->mNumChannels;

	Concurrency::parallel_for(0, currChannelNum, [anim, this](int index)
		{
			auto currChannel = anim->mChannels[index];
			auto nodeIter = m_currNodeTree.find(currChannel->mNodeName.C_Str());
			if (nodeIter != m_currNodeTree.end())
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
					if (prevTime < m_currTime && nextTime > m_currTime)
					{
						double spot = (m_currTime - prevTime) / (nextTime - prevTime);
						xmScale = XMVectorLerp(XMLoadFloat3(reinterpret_cast<const XMFLOAT3*>(&currChannel->mScalingKeys[i].mValue)),
							XMLoadFloat3(reinterpret_cast<const XMFLOAT3*>(&currChannel->mScalingKeys[i + 1].mValue)), spot);
						break;
					}
				}

				for (int i = 0; i < numscale - 1; i++)
				{
					double prevTime = currChannel->mScalingKeys[i].mTime;
					double nextTime = currChannel->mScalingKeys[i + 1].mTime;
					if (prevTime < m_currTime && nextTime > m_currTime)
					{
						double spot = (m_currTime - prevTime) / (nextTime - prevTime);
						xmRotate = XMQuaternionSlerp(XMLoadFloat4(reinterpret_cast<const XMFLOAT4*>(&currChannel->mRotationKeys[i].mValue)),
							XMLoadFloat4(reinterpret_cast<const XMFLOAT4*>(&currChannel->mRotationKeys[i + 1].mValue)), spot);
						break;
					}
				}

				for (int i = 0; i < numPos - 1; i++)
				{
					double prevTime = currChannel->mPositionKeys[i].mTime;
					double nextTime = currChannel->mPositionKeys[i + 1].mTime;
					if (prevTime < m_currTime && nextTime > m_currTime)
					{
						double spot = (m_currTime - prevTime) / (nextTime - prevTime);
						xmPos = XMVectorLerp(XMLoadFloat3(reinterpret_cast<const XMFLOAT3*>(&currChannel->mPositionKeys[i].mValue)),
							XMLoadFloat3(reinterpret_cast<const XMFLOAT3*>(&currChannel->mPositionKeys[i + 1].mValue)), spot);
						break;
					}
				}

				auto nodeMat = XMLoadFloat4x4(&nodeIter->second->m_srt);
				auto nodeAniMat = XMMatrixAffineTransformation(xmScale, XMVectorZero(), xmRotate, xmPos);

				XMStoreFloat4x4(&nodeIter->second->m_srt, nodeAniMat * nodeMat);
			}
		});

}

void COMAnimator::MeshAnimation(const aiAnimation* anim, CGHNode* node)
{
}

void COMAnimator::MorphAnimation(const aiAnimation* anim, CGHNode* node)
{
}

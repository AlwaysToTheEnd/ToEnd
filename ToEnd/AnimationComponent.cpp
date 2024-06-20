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
			std::bind(&COMAnimator::NodeAnimation, this, currAnimation, 0),
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

void COMAnimator::AnimationRigging(CGHAnimationGroup* group, const std::unordered_map<std::string, DirectX::XMFLOAT4X4>& baseMeshBones)
{
	assert(group->rigMapping);

	struct DisplacementData
	{
		DirectX::XMFLOAT3 pos;
		DirectX::XMFLOAT3 scale;
		DirectX::XMFLOAT4 rotate;
	};
	static std::unordered_map<std::string, DisplacementData> displacementDatas;

	auto rigMapping = group->rigMapping;

	if (group->boneOffsetMatrices.get())
	{
		displacementDatas.clear();

		const auto aniBones = group->boneOffsetMatrices.get();

		for (auto& iter : *rigMapping)
		{
			auto baseIter = baseMeshBones.find(iter.second);
			auto aniIter = aniBones->find(iter.first);

			if (baseIter != baseMeshBones.end() && aniIter != aniBones->end())
			{
				DirectX::XMMATRIX baseMat = DirectX::XMLoadFloat4x4(&baseIter->second);
				DirectX::XMMATRIX aniMat = DirectX::XMLoadFloat4x4(&aniIter->second.offsetMat);

				DirectX::XMVECTOR baseScale = DirectX::XMVectorZero();
				DirectX::XMVECTOR baseRotate = DirectX::XMVectorSet(0, 0, 0, 1);
				DirectX::XMVECTOR basePos = DirectX::XMVectorZero();

				DirectX::XMVECTOR aniScale = DirectX::XMVectorZero();
				DirectX::XMVECTOR aniRotate = DirectX::XMVectorSet(0, 0, 0, 1);
				DirectX::XMVECTOR aniPos = DirectX::XMVectorZero();

				DirectX::XMMatrixDecompose(&baseScale, &baseRotate, &basePos, baseMat);
				DirectX::XMMatrixDecompose(&aniScale, &aniRotate, &aniPos, aniMat);

				DirectX::XMVECTOR displacementPosRatio = DirectX::XMVectorDivide(basePos, aniPos);
				DirectX::XMVECTOR displacementScaleRatio = DirectX::XMVectorDivide(baseScale, aniScale);
				DirectX::XMVECTOR displacementRotate = DirectX::XMQuaternionMultiply(DirectX::XMQuaternionInverse(aniRotate), baseRotate);

				DirectX::XMFLOAT3 rotateValue = {};
				CGH::QuaternionToAngularAngles(displacementRotate, rotateValue.x , rotateValue.y, rotateValue.z);

				DisplacementData data = {};
				DirectX::XMStoreFloat3(&data.pos, displacementPosRatio);
				DirectX::XMStoreFloat3(&data.scale, displacementScaleRatio);
				DirectX::XMStoreFloat4(&data.rotate, displacementRotate);

				displacementDatas.insert({ iter.first, data });
			}
			else
			{
				assert(false);
			}
		}

		for (auto& currAnim : group->anims)
		{
			unsigned int currChannelNum = currAnim.mNumChannels;

			for (int i = 0; i < currChannelNum; i++)
			{
				auto currChannel = currAnim.mChannels[i];
				auto displacementDataIter = displacementDatas.find(currChannel->mNodeName.C_Str());

				if (displacementDataIter != displacementDatas.end())
				{
					const auto& displacementData = displacementDataIter->second;

					for (int j = 0; j < currChannel->mNumScalingKeys; j++)
					{
						currChannel->mScalingKeys[j].mValue.x *= displacementData.scale.x;
						currChannel->mScalingKeys[j].mValue.y *= displacementData.scale.y;
						currChannel->mScalingKeys[j].mValue.z *= displacementData.scale.z;
					}

					for (int j = 0; j < currChannel->mNumRotationKeys; j++)
					{
						aiQuaternion& currQuat = currChannel->mRotationKeys[j].mValue;
						DirectX::XMVECTOR currQuatVec = DirectX::XMVectorSet(currQuat.x, currQuat.y, currQuat.z, currQuat.w);
						DirectX::XMVECTOR displacementQuat = DirectX::XMLoadFloat4(&displacementData.rotate);
						DirectX::XMVECTOR resultQuat = DirectX::XMQuaternionMultiply(displacementQuat, currQuatVec);
						resultQuat = DirectX::XMQuaternionMultiply(resultQuat, DirectX::XMQuaternionInverse(displacementQuat));

						currQuat.x = DirectX::XMVectorGetX(resultQuat);
						currQuat.y = DirectX::XMVectorGetY(resultQuat);
						currQuat.z = DirectX::XMVectorGetZ(resultQuat);
						currQuat.w = DirectX::XMVectorGetW(resultQuat);
					}

					for (int j = 0; j < currChannel->mNumPositionKeys; j++)
					{
						currChannel->mPositionKeys[j].mValue.x *= displacementData.pos.x;
						currChannel->mPositionKeys[j].mValue.y *= displacementData.pos.y;
						currChannel->mPositionKeys[j].mValue.z *= displacementData.pos.z;
					}
				}
				else
				{
					assert(false);
				}
			}
		}
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

#pragma once
#include <list>
#include "Component.h"
#include "CGHGraphicResource.h"

enum class ANIMATION_PARAMETER_TYPE
{
	ANIMATION_PRAM_TYPE_FLOAT,
	ANIMATION_PRAM_TYPE_INT,
	ANIMATION_PRAM_TYPE_BOOL,
};

struct CGHAnimationParameter
{
	ANIMATION_PARAMETER_TYPE type = ANIMATION_PARAMETER_TYPE::ANIMATION_PRAM_TYPE_FLOAT;
	union
	{
		float vFloat;
		int vInt;
		bool vBool;
	};
};

struct CGHAnimationState
{
	std::string name;
	float speed = 1.0f;
	const aiAnimation* targetAnimation = nullptr;
};

class COMAnimator : public Component
{
public:
	COMAnimator(CGHNode* node);

	virtual void Release(CGHNode* ndoe);
	virtual void Update(CGHNode* node, unsigned int, float delta) override;
	virtual void RateUpdate(CGHNode* node, unsigned int, float delta) override {}
	virtual size_t GetTypeHashCode() override { return s_hashCode; }
	virtual unsigned int GetPriority() override { return COMPONENT_ANIMATOR; }

	void SetAnimation(const aiAnimation* anim, unsigned int stateIndex);

private:
	void NodeTreeDirty();
	void NodeAnimation(const aiAnimation* anim);
	void MeshAnimation(const aiAnimation* anim, CGHNode* node);
	void MorphAnimation(const aiAnimation* anim, CGHNode* node);

private:
	static size_t s_hashCode;
	const CGHAnimationGroup* m_currGroup = nullptr;
	const CGHAnimationState* m_currState = nullptr;
	double m_currTime = 0.0;

	bool m_nodeTreeDirty = true;
	std::unordered_map<std::string, CGHNode*> m_currNodeTree;
	std::unordered_map<std::string, CGHNode*> m_rigMap;

	std::vector<std::vector<unsigned int>> m_animationTransitionInfos;
	std::unordered_map<std::string, CGHAnimationParameter> m_params;
	std::vector<CGHAnimationState> m_states;
};


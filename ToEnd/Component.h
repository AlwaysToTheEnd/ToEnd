#pragma once
#include <typeinfo>
#include <DirectXMath.h>

enum COMPONENTPRIORITY
{
	COMPONENT_TRANSFORM,
	COMPONENT_MATERIAL,
	COMPONENT_SKINNEDMESH,
	COMPONENT_MESH_RENDERER,
	COMPONENT_SKINNEDMESH_RENDERER,
	COMPONENT_UI_RENDERER,
	COMPONENT_CUSTOM,
};

class CGHNode;

class Component
{
public:
	virtual ~Component() = default;
	virtual void Release(CGHNode* ndoe) {};
	virtual void Update(CGHNode* node, unsigned int currFrame, float delta) = 0;
	virtual void RateUpdate(CGHNode* node, unsigned int currFrame, float delta) = 0;
	virtual size_t GetTypeHashCode() = 0;
	virtual unsigned int GetPriority() { return COMPONENT_CUSTOM; }

	void SetActvie(bool value) { m_active = value; }
	bool GetActive() { return m_active; }

protected:
	bool m_active = true;
};

#pragma once
#include <typeinfo>
#include <functional>
#include <vector>
#include <DirectXMath.h>

enum COMPONENTPRIORITY
{
	COMPONENT_TRANSFORM,
	COMPONENT_MATERIAL,
	COMPONENT_SKINNEDMESH,
	COMPONENT_MESH_RENDERER,
	COMPONENT_SKINNEDMESH_RENDERER,
	COMPONENT_UITRANSFORM,
	COMPONENT_ANIMATOR,
	COMPONENT_CUSTOM,
};

class CGHNode;

class Component
{
public:
	virtual ~Component()
	{
		for (auto& iter : m_deleteEvent)
		{
			iter(this);
		}
	}
	virtual void Release(CGHNode* ndoe) {};
	virtual void Update(CGHNode* node, unsigned int currFrame, float delta) = 0;
	virtual void RateUpdate(CGHNode* node, unsigned int currFrame, float delta) = 0;
	virtual size_t GetTypeHashCode() = 0;
	virtual unsigned int GetPriority() { return COMPONENT_CUSTOM; }

	void AddDeleteEvent(std::function<void(Component*)> func)
	{
		m_deleteEvent.emplace_back(func);
	}
	void SetActvie(bool value) { m_active = value; }
	bool GetActive() { return m_active; }

protected:
	bool m_active = true;
	std::vector<std::function<void(Component*)>> m_deleteEvent;
};

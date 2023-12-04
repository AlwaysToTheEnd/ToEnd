#pragma once
#include <vector>
#include <unordered_map>
#include <string>
#include <memory>
#include <DirectXMath.h>
#include "Component.h"

class CGHNode
{
public:
	CGHNode(const char* name);
	virtual ~CGHNode();

	virtual void Update(float delta);
	virtual void RateUpdate(float delta);

	virtual void Render();
	virtual void OnClcked();
	virtual void OnMouseOvered();

	template<typename T> T* CreateComponent();
	template<typename T> T* GetComponent();

	int GetObjectID() { return m_objectID; }
	bool GetActive() { return m_active; }
	CGHNode* GetParent() { return m_parent; }
	const char* GetaName() { return m_name.c_str(); }

	void SetActive(bool isActive) const { isActive = m_active; }
	void SetParent(CGHNode* parent);
	void SetName(const char* name) { m_name = name; }

public:
	DirectX::XMFLOAT4X4 m_srt;

protected:
	std::string m_name;

	CGHNode* m_parent = nullptr;
	std::unique_ptr<Transform> m_transformComponent;
	std::vector<CGHNode*> m_childs;
	std::vector<std::unique_ptr<Component>> m_components;

private:
	bool m_active = true;
	int m_objectID = 0;
	static std::vector<int> s_remainingIDs;
	static std::vector<CGHNode*> s_allObjects;
	static size_t s_currMaxID;
};

template<typename T>
inline T* CGHNode::CreateComponent()
{
	Component* result = new T(this);
	std::unique_ptr<Component> uniqueTemp(result);
	m_components.push_back(std::move(uniqueTemp));
	
	return result;
}

template<>
inline Transform* CGHNode::CreateComponent()
{
	Transform* result = new Transform(this);
	std::unique_ptr<Transform> uniqueTemp(result);

	m_transformComponent= std::move(uniqueTemp);

	return result;
}

template<typename T>
inline T* CGHNode::GetComponent()
{
	T* result = nullptr;
	
	for (auto& iter : m_components)
	{
		if (iter->GetTypeHashCode() == typeid(T).hash_code())
		{
			result = iter.get();
		}
	}

	return result;
}

template<>
inline Transform* CGHNode::GetComponent()
{
	return m_transformComponent.get();
}
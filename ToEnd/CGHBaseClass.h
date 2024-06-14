#pragma once
#include <vector>
#include <unordered_map>
#include <string>
#include <memory>
#include <DirectXMath.h>
#include <algorithm>
#include <functional>
#include "BaseComponents.h"
#include "AnimationComponent.h"

enum CGHNODE_EVENT_FLAGS
{
	CGHNODE_EVENT_FLAG_NONE = 0,
	CGHNODE_EVENT_FLAG_CHILDTREE_CHANGED = 1,
	CGHNODE_EVENT_FLAG_DELETE = 2,
};

enum class CGHNODE_LAYER
{
	CGHNODE_LYAER_DEFAULT = 0,
	CGHNODE_LYAER_CHARACTER,
	CGHNODE_LYAER_MAP,
	CGHNODE_LYAER_NUM,
};

class CGHNode
{
	struct CGHNodeEvent
	{
		std::function<void()> func;
		int eventFlags;
	};

public:
	CGHNode();
	CGHNode(const CGHNode& rhs);
	virtual ~CGHNode();

	virtual void Init() {};
	virtual void Update(float delta);
	virtual void RateUpdate(float delta);
	virtual void Render(unsigned int currFrame);
	virtual void RenderGUI(unsigned int currFrame);

	template<typename T> T* CreateComponent();
	template<typename T> T* GetComponent();
	void GetHasComponents(std::vector<Component*>& outComps);

	bool GetActive() const { return m_active; }
	CGHNode* GetParent() const { return m_parent; }
	CGHNode* FindNode(const char* name);
	const char* GetName() const { return m_name.c_str(); }
	void GetChildNodes(std::vector<CGHNode*>* nodeOut);
	std::vector<CGHNode*>& GetChilds() { return m_childs; }
	CGHNode* GetRootNode();
	CGHNODE_LAYER GetLayer() const { return m_nodeLayer; }

	void AddEvent(std::function<void()> func, int flags) { m_events.push_back({ func, flags }); }
	void ClearEvents() { m_events.clear(); }
	void RemoveEvent(std::function<void()> func, int flags);
	void SetActive(bool isActive) const { isActive = m_active; }
	void SetParent(CGHNode* parent, bool denyEvent = false);
	void SetName(const char* name) { m_name = name; }
	void SetLayer(CGHNODE_LAYER layer) { m_nodeLayer = layer; }

protected:
	void ExcuteEvent(CGHNODE_EVENT_FLAGS flag);

public:
	DirectX::XMFLOAT4X4 m_srt;

protected:
	std::string m_name;
	bool m_active = true;
	CGHNode* m_parent = nullptr;
	CGHNODE_LAYER m_nodeLayer = CGHNODE_LAYER::CGHNODE_LYAER_DEFAULT;
	std::vector<CGHNode*> m_childs;
	std::vector<CGHNodeEvent> m_events;
	std::vector<std::unique_ptr<Component>> m_components;
};

template<>
inline COMTransform* CGHNode::CreateComponent()
{
	COMTransform* result = new COMTransform(this);
	std::unique_ptr<Component> uniqueTemp(result);

	m_components[COMPONENT_TRANSFORM] = std::move(uniqueTemp);

	return result;
}

template<>
inline COMTransform* CGHNode::GetComponent()
{
	return reinterpret_cast<COMTransform*>(m_components[COMPONENT_TRANSFORM].get());
}

template<>
inline COMSkinnedMesh* CGHNode::CreateComponent()
{
	COMSkinnedMesh* result = new COMSkinnedMesh(this);
	std::unique_ptr<Component> uniqueTemp(result);

	m_components[COMPONENT_SKINNEDMESH] = std::move(uniqueTemp);

	return result;
}

template<>
inline COMSkinnedMesh* CGHNode::GetComponent()
{
	return reinterpret_cast<COMSkinnedMesh*>(m_components[COMPONENT_SKINNEDMESH].get());
}

template<>
inline COMMaterial* CGHNode::CreateComponent()
{
	COMMaterial* result = new COMMaterial(this);
	std::unique_ptr<Component> uniqueTemp(result);

	m_components[COMPONENT_MATERIAL] = std::move(uniqueTemp);

	return result;
}

template<>
inline COMMaterial* CGHNode::GetComponent()
{
	return reinterpret_cast<COMMaterial*>(m_components[COMPONENT_MATERIAL].get());
}

template<>
inline COMDX12SkinnedMeshRenderer* CGHNode::CreateComponent()
{
	COMDX12SkinnedMeshRenderer* result = new COMDX12SkinnedMeshRenderer(this);
	std::unique_ptr<Component> uniqueTemp(result);

	m_components[COMPONENT_SKINNEDMESH_RENDERER] = std::move(uniqueTemp);

	return result;
}

template<>
inline COMDX12SkinnedMeshRenderer* CGHNode::GetComponent()
{
	return reinterpret_cast<COMDX12SkinnedMeshRenderer*>(m_components[COMPONENT_SKINNEDMESH_RENDERER].get());
}

template<>
inline COMAnimator* CGHNode::CreateComponent()
{
	COMAnimator* result = new COMAnimator(this);
	std::unique_ptr<Component> uniqueTemp(result);

	m_components[COMPONENT_ANIMATOR] = std::move(uniqueTemp);

	return result;
}

template<>
inline COMAnimator* CGHNode::GetComponent()
{
	return reinterpret_cast<COMAnimator*>(m_components[COMPONENT_ANIMATOR].get());
}

template<typename T>
inline T* CGHNode::CreateComponent()
{
	T* result = new T(this);
	std::unique_ptr<Component> uniqueTemp(result);
	m_components.push_back(std::move(uniqueTemp));

	return result;
}

template<typename T>
inline T* CGHNode::GetComponent()
{
	T* result = nullptr;

	auto typeHashCode = typeid(T).hash_code();
	for (size_t i = m_components.size() - 1; i >= COMPONENT_CUSTOM; i--)
	{
		if (m_components[i]->GetTypeHashCode() == typeHashCode)
		{
			result = reinterpret_cast<T*>(m_components[i].get());
			break;
		}
	}

	return result;
}

namespace CGH
{
	inline void FixEpsilonMatrix(DirectX::XMFLOAT4X4& matrix, float epsilon = 1.192092896e-7f)
	{
		float* currFloat = &matrix._11;

		for (int j = 0; j < 16; j++)
		{
			if (j % 4 == j / 4)
			{
				if (std::abs(std::abs(currFloat[j]) - 1.0f) < epsilon)
				{
					currFloat[j] = 1.0f;
				}
			}
			else
			{
				if (std::abs(currFloat[j]) < epsilon)
				{
					currFloat[j] = 0;
				}
			}
		}
	}
}




#pragma once
#include <vector>
#include <unordered_map>
#include <string>
#include <memory>
#include <DirectXMath.h>
#include <algorithm>
#include <functional>
#include "Component.h"

enum CGHNODE_EVENTFLAGS
{
	CGHNODE_EVENT_FLAG_NONE = 0,
	CGHNODE_EVENT_FLAG_ROOT_TREE_CHANGED = 1,
	CGHNODE_EVENT_FLAG_END = 0xffffffff,
};

static struct GlobalOptions
{
	struct WindowOption
	{
		int WindowsizeX = 900;
		int WindowsizeY = 900;
	}WIN;

	struct GraphicOption
	{
		const std::wstring	TextureFolderPath = L"./../Common/Texture";
		const std::wstring	FontFolderPath = L"./../Common/Fonts";
		const std::wstring	SpriteTextureSuffix = L"sp_";
	}GRAPHIC;
}GO;

class CGHNode
{
public:
	CGHNode();
	CGHNode(const CGHNode& rhs);
	virtual ~CGHNode();

	virtual void Update(unsigned int currFrame, float delta);
	virtual void RateUpdate(unsigned int currFrame, float delta);

	virtual void OnClcked();
	virtual void OnMouseOvered();

	template<typename T> T* CreateComponent();
	template<typename T> T* GetComponent();

	bool GetActive() const { return m_active; }
	CGHNode* GetParent() const { return m_parent; }
	CGHNode* GetRoot() const { return m_root; }
	const char* GetaName() const { return m_name.c_str(); }
	void GetChildNodes(std::vector<const CGHNode*>* nodeOut) const;
	bool CheckNodeEvent(CGHNode* idNode, CGHNODE_EVENTFLAGS flag) const;

	void SetActive(bool isActive) const { isActive = m_active; }
	void SetParent(CGHNode* parent);
	void SetName(const char* name) { m_name = name; }

	static void ClearNodeEvents();

private:
	void SetRoot(CGHNode* node);

public:
	DirectX::XMFLOAT4X4 m_srt;

private:
	static std::unordered_map<CGHNode*, int> s_nodeEvents;
	std::string m_name;
	bool m_active = true;
	CGHNode* m_parent = nullptr;
	CGHNode* m_root = this;
	std::vector<CGHNode*> m_childs;
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

template<typename T>
inline T* CGHNode::CreateComponent()
{
	T* result = new T(this);
	std::unique_ptr<Component> uniqueTemp(result);
	m_components.push_back(std::move(uniqueTemp));

	for (size_t i = m_components.size() - 1; i >= COMPONENT_CUSTOM; i--)
	{
		if (m_components[i - 1]->GetPriority() < m_components[i]->GetPriority())
		{
			std::swap(m_components[i - 1], m_components[i]);
		}
		else
		{
			break;
		}
	}

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
			result = reinterpret_cast<T*>(iter.get());
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




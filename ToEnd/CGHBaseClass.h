#pragma once
#include <vector>
#include <unordered_map>
#include <string>
#include <memory>
#include <DirectXMath.h>
#include <algorithm>
#include "Component.h"

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

	bool GetActive() { return m_active; }
	CGHNode* GetParent() { return m_parent; }
	const char* GetaName() { return m_name.c_str(); }

	const std::unordered_map<std::string, CGHNode*>* GetNodeTree();

	void SetActive(bool isActive) const { isActive = m_active; }
	void SetParent(CGHNode* parent);
	void SetName(const char* name) { m_name = name; }

public:
	DirectX::XMFLOAT4X4 m_srt;

protected:
	std::string m_name;

	CGHNode* m_parent = nullptr;
	std::unique_ptr<COMTransform> m_transformComponent;
	std::vector<CGHNode*> m_childs;
	std::vector<std::unique_ptr<Component>> m_components;

private:
	bool m_active = true;
};

template<typename T>
inline T* CGHNode::CreateComponent()
{
	Component* result = new T(this);
	std::unique_ptr<Component> uniqueTemp(result);
	m_components.push_back(std::move(uniqueTemp));

	for (size_t i = m_components.size() - 1; i > 0; i--)
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

template<>
inline COMTransform* CGHNode::CreateComponent()
{
	COMTransform* result = new COMTransform();
	std::unique_ptr<COMTransform> uniqueTemp(result);

	m_transformComponent = std::move(uniqueTemp);

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
			break;
		}
	}

	return result;
}

template<>
inline COMTransform* CGHNode::GetComponent()
{
	return m_transformComponent.get();
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

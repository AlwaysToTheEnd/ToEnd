#include "CGHBaseClass.h"

size_t CGHNode::s_currMaxID = 0;
std::vector<int> CGHNode::s_remainingIDs;
std::vector<CGHNode*> CGHNode::s_allObjects;

CGHNode::CGHNode(const char* name)
{
	m_name = name;
	DirectX::XMStoreFloat4x4(&m_srt, DirectX::XMMatrixIdentity());

	if (s_remainingIDs.size())
	{
		m_objectID = s_remainingIDs.back();
		s_remainingIDs.pop_back();
		s_allObjects[m_objectID] = this;
	}
	else
	{
		m_objectID = s_currMaxID;
		s_currMaxID++;
		s_allObjects.push_back(this);
	}
}

CGHNode::~CGHNode()
{
	s_remainingIDs.push_back(m_objectID);
	s_allObjects[m_objectID] = nullptr;
}

void CGHNode::Update(float delta)
{
	if (m_active)
	{
		if (m_transformComponent != nullptr)
		{
			m_transformComponent->Update(delta);
		}

		for (auto& iter : m_childs)
		{
			iter->Update(delta);
		}
	}
}

void CGHNode::RateUpdate(float delta)
{
	if (m_active)
	{
		if (m_transformComponent != nullptr)
		{
			m_transformComponent->RateUpdate(delta);
		}

		for (auto& iter : m_childs)
		{
			iter->RateUpdate(delta);
		}
	}
}

void CGHNode::Render()
{
	if (m_active)
	{
		for (auto& iter : m_childs)
		{
			iter->Render();
		}
	}
}

void CGHNode::OnClcked()
{

}

void CGHNode::OnMouseOvered()
{
}

void CGHNode::SetParent(CGHNode* parent)
{
	if (m_parent != parent)
	{
		if (m_parent)
		{
			for (size_t i = 0; i < m_parent->m_childs.size(); i++)
			{
				if (m_parent->m_childs[i] == this)
				{
					m_parent->m_childs[i] = m_parent->m_childs.back();
					m_parent->m_childs.pop_back();
					break;
				}
			}

			m_parent = parent;

			if (m_parent)
			{
				m_parent->m_childs.push_back(this);
			}
		}
	}
}


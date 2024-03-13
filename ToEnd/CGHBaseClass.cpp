#include <assert.h>
#include "CGHBaseClass.h"

CGHNode::CGHNode()
{
	DirectX::XMStoreFloat4x4(&m_srt, DirectX::XMMatrixIdentity());
}

CGHNode::CGHNode(const CGHNode& rhs)
{
	assert(false);
}

CGHNode::~CGHNode()
{
	
}


void CGHNode::Update(unsigned int currFrame, float delta)
{
	if (m_active)
	{
		if (m_transformComponent != nullptr)
		{
			m_transformComponent->Update(this, currFrame, delta);
		}

		for (auto& iter : m_components)
		{
			iter->Update(this, currFrame, delta);
		}

		for (auto& iter : m_childs)
		{
			iter->Update(currFrame, delta);
		}
	}
}

void CGHNode::RateUpdate(unsigned int currFrame, float delta)
{
	if (m_active)
	{
		for (auto& iter : m_components)
		{
			iter->RateUpdate(this, currFrame, delta);
		}

		for (auto& iter : m_childs)
		{
			iter->RateUpdate(currFrame, delta);
		}
	}
}

void CGHNode::OnClcked()
{

}

void CGHNode::OnMouseOvered()
{
}

const std::unordered_map<std::string, CGHNode*>* CGHNode::GetNodeTree()
{

	return nullptr;
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
		}

		m_parent = parent;

		if (m_parent)
		{
			m_parent->m_childs.push_back(this);
		}
	}
}


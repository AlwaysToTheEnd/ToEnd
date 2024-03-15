#include <assert.h>
#include "CGHBaseClass.h"

CGHNode::CGHNode()
{
	DirectX::XMStoreFloat4x4(&m_srt, DirectX::XMMatrixIdentity());

	m_components.resize(COMPONENT_CUSTOM);
}

CGHNode::CGHNode(const CGHNode& rhs)
{
	assert(false);
}

CGHNode::~CGHNode()
{
	for (auto& iter : m_components)
	{
		iter->Release(this);
	}
}


void CGHNode::Update(unsigned int currFrame, float delta)
{
	if (m_active)
	{
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

void CGHNode::GetChildNodes(std::vector<const CGHNode*>* nodeOut) const
{
	nodeOut->push_back(this);

	for (auto& iter : m_childs)
	{
		iter->GetChildNodes(nodeOut);
	}
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

			m_parent->ChildNodeTreeChangeEvent();
		}

		m_parent = parent;

		if (m_parent)
		{
			m_parent->m_childs.push_back(this);
			m_parent->ChildNodeTreeChangeEvent();
		}
	}
}

void CGHNode::ChildNodeTreeChangeEvent()
{
	if (m_parent)
	{
		m_parent->ChildNodeTreeChangeEvent();
	}

	for (auto& iter : m_childNodeTreeChangeEvent)
	{
		iter.func();
	}
}

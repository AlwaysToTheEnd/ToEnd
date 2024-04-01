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
		if (iter.get())
		{
			iter->Release(this);
		}
	}
}


void CGHNode::Update(unsigned int currFrame, float delta)
{
	if (m_active)
	{
		for (auto& iter : m_components)
		{
			if (iter.get())
			{
				iter->Update(this, currFrame, delta);
			}
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
			if (iter.get())
			{
				iter->RateUpdate(this, currFrame, delta);
			}
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

void CGHNode::SetParent(CGHNode* parent, bool denyEvent)
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

			if (!denyEvent)
			{
				CGHNode* tempNode = m_parent;

				while (tempNode)
				{
					tempNode->ExcuteEvent(CGHNODE_EVENT_FLAG_CHILDTREE_CHANGED);
				}
			}
		}

		m_parent = parent;

		if (m_parent)
		{
			m_parent->m_childs.push_back(this);
		}

		if (!denyEvent)
		{
			CGHNode* tempNode = m_parent;

			while (tempNode)
			{
				tempNode->ExcuteEvent(CGHNODE_EVENT_FLAG_CHILDTREE_CHANGED);
			}
		}
	}
}

void CGHNode::ExcuteEvent(CGHNODE_EVENT_FLAGS flag)
{
	for (auto& iter : m_evnets)
	{
		if (flag && iter.eventFlags)
		{
			iter.func();
		}
	}
}




#include <assert.h>
#include "CGHBaseClass.h"

std::unordered_map<CGHNode*, int> CGHNode::s_nodeEvents;

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

bool CGHNode::CheckNodeEvent(CGHNode* idNode, CGHNODE_EVENTFLAGS flag) const
{
	auto iter = s_nodeEvents.find(idNode);

	if (iter != s_nodeEvents.end())
	{
		return iter->second && flag;
	}

	return false;
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

			s_nodeEvents[m_parent->GetRoot()] |= CGHNODE_EVENT_FLAG_ROOT_TREE_CHANGED;
		}

		m_parent = parent;

		if (m_parent)
		{
			m_root = m_parent->GetRoot();
			m_parent->m_childs.push_back(this);
			SetRoot(m_parent->m_root);
		}
		else
		{
			SetRoot(this);
		}

		s_nodeEvents[GetRoot()] |= CGHNODE_EVENT_FLAG_ROOT_TREE_CHANGED;
	}
}

void CGHNode::ClearNodeEvents()
{
	s_nodeEvents.clear();
}

void CGHNode::SetRoot(CGHNode* node)
{
	m_root = node;

	for (auto& iter : m_childs)
	{
		iter->SetRoot(node);
	}
}



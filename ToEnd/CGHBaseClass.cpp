#include <assert.h>
#include "CGHBaseClass.h"
#include "../Common/Source/CGHUtil.h"
#include "imgui.h"


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
	ExcuteEvent(CGHNODE_EVENT_FLAG_DELETE);

	for (auto& iter : m_components)
	{
		if (iter.get())
		{
			iter->Release(this);
		}
	}
}

void CGHNode::Update(float delta)
{
	if (m_active)
	{
		for (auto& iter : m_components)
		{
			if (iter.get() && iter->GetActive())
			{
				iter->Update(this, delta);
			}
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
		for (auto& iter : m_components)
		{
			if (iter.get() && iter->GetActive())
			{
				iter->RateUpdate(this, delta);
			}
		}

		for (auto& iter : m_childs)
		{
			iter->RateUpdate(delta);
		}
	}
}

void CGHNode::Render(unsigned int currFrame)
{
	if (m_active)
	{
		for (auto& iter : m_components)
		{
			if (iter.get() && iter->GetActive())
			{
				iter->Render(this, currFrame);
			}
		}

		for (auto& iter : m_childs)
		{
			iter->Render(currFrame);
		}
	}

	std::memcpy(&m_srt, &CGH::IdentityMatrix, sizeof(m_srt));
}

void CGHNode::RenderGUI(unsigned int currFrame)
{
	ImGui::BeginChild(m_name.c_str(), ImVec2(0,0), ImGuiChildFlags_Border);
	ImGui::Checkbox(m_name.c_str(), &m_active);
	ImGui::Text("Layer : %d", m_nodeLayer);
	ImGui::Spacing();

	ImGui::BeginChild("Components", ImVec2(0, 0), ImGuiChildFlags_Border);
	for (auto& iter : m_components)
	{
		if (iter.get())
		{
			iter->GUIRender(currFrame, reinterpret_cast<unsigned int>(iter.get()));
		}
	}

	ImGui::EndChild();
	ImGui::EndChild();
}

CGHNode* CGHNode::FindNode(const char* name)
{
	CGHNode* result = nullptr;

	if (m_name == name)
	{
		return this;
	}
	else
	{
		for (auto child : m_childs)
		{
			result = child->FindNode(name);

			if (result)
			{
				break;
			}
		}
	}

	return result;
}

void CGHNode::GetChildNodes(std::vector<CGHNode*>* nodeOut)
{
	nodeOut->push_back(this);

	for (auto& iter : m_childs)
	{
		iter->GetChildNodes(nodeOut);
	}
}

CGHNode* CGHNode::GetRootNode()
{
	CGHNode* result = this;

	if (m_parent)
	{
		result = m_parent->GetRootNode();
	}

	return result;
}

void CGHNode::RemoveEvent(std::function<void()> func, int flags)
{
	for (auto iter = m_events.begin(); iter != m_events.end(); iter++)
	{
		if (iter->func.target_type() == func.target_type() && flags == iter->eventFlags)
		{
			m_events.erase(iter);
			break;
		}
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
	for (auto& iter : m_events)
	{
		if (flag && iter.eventFlags)
		{
			iter.func();
		}
	}
}




#include "NodeController.h"
#include "CGHNodePicker.h"
#include "../Common/Source/CGHUtil.h"
#include "imgui.h"
#include "imgui_internal.h"

NodeController::NodeController()
{
}

NodeController::~NodeController()
{
}

void NodeController::Update(float delta)
{
	if (m_active)
	{
		auto pickedNode = CGHNodePicker::s_instance.GetCurrPickedNode();
		
		if (pickedNode)
		{
			if (pickedNode != m_currSelected)
			{
				if (m_currSelected)
				{
					m_currSelected->RemoveEvent(std::bind(&NodeController::SelectedNodeRemoved, this, m_currSelected), CGHNODE_EVENT_FLAG_DELETE);
				}

				m_currSelected = pickedNode;
				m_currSelected->AddEvent(std::bind(&NodeController::SelectedNodeRemoved, this, m_currSelected), CGHNODE_EVENT_FLAG_DELETE);
			}
		}
	}
}

void NodeController::RenderGUI(unsigned int currFrame)
{
	if (m_active)
	{
		if (!ImGui::Begin("NodeController", &m_active))
		{
			ImGui::End();
			return;
		}

		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2, 2));

		if (ImGui::BeginTable("NodeListTable", 1, ImGuiTableFlags_RowBg |
			ImGuiTableFlags_ScrollY | ImGuiTableFlags_ScrollX, ImVec2(ImGui::GetContentRegionAvail().x * 0.5, ImGui::GetContentRegionAvail().y)))
		{
			ImGui::TableSetupScrollFreeze(0, 1);
			ImGui::TableSetupColumn("Nodes");
			ImGui::TableHeadersRow();
			
			for(auto iter : m_rootNodeList)
			{
				RenderNodeTree(iter, iter);
			}

			ImGui::EndTable();
		}

		ImGui::SameLine();

		if (m_currSelected)
		{
			m_currSelected->RenderGUI(currFrame);
		}

		ImGui::PopStyleVar();
		ImGui::End();
	}
}

void NodeController::RenderRootNodes(std::vector<CGHNode*>& rootNodes)
{
	m_rootNodeList.assign(rootNodes.begin(), rootNodes.end());
}

void NodeController::RenderNodeTree(CGHNode* node, void* uid)
{
	ImGui::PushID(uid);
	ImGui::TableNextRow();
	ImGui::TableSetColumnIndex(0);
	ImGui::AlignTextToFramePadding();

	ImGuiTreeNodeFlags flags = node == m_currSelected ? ImGuiTreeNodeFlags_Selected : ImGuiTreeNodeFlags_None;
	flags |= ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;

	if (node->GetChilds().size() == 0)
	{
		flags |= ImGuiTreeNodeFlags_Leaf;
	}

	bool opned = ImGui::TreeNodeEx(node->GetName(), flags);

	if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
	{
		CGHNodePicker::s_instance.PickNode(node);

		if (m_currSelected != node)
		{
			if (m_currSelected)
			{
				m_currSelected->RemoveEvent(std::bind(&NodeController::SelectedNodeRemoved, this, m_currSelected), CGHNODE_EVENT_FLAG_DELETE);
			}
		}

		m_currSelected = node;
		m_currSelected->AddEvent(std::bind(&NodeController::SelectedNodeRemoved, this, m_currSelected), CGHNODE_EVENT_FLAG_DELETE);
	}

	if (opned)
	{
		unsigned int index = 0;
		for (auto iter : node->GetChilds())
		{
			RenderNodeTree(iter, iter);
		}

		ImGui::TreePop();
	}

	ImGui::PopID();
}

void NodeController::SelectedNodeRemoved(CGHNode* node)
{
	if (m_currSelected == node)
	{
		m_currSelected = nullptr;
	}
}

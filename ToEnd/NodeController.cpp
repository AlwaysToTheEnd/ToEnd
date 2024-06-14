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
		auto currPicked = CGHNodePicker::s_instance.GetCurrPickedNode();

		if (m_currTarget != currPicked)
		{
			m_currTarget = currPicked;
			m_currSelected = nullptr;
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

			if (m_currTarget)
			{
				RenderNodeTree(m_currTarget, 0);
			}

			ImGui::EndTable();
		}

		ImGui::SameLine();

		if (ImGui::BeginChild("ComponentsInfo"))
		{
			if (m_currTarget && m_currSelected)
			{
				ImGui::Text(m_currSelected->GetName());
				std::vector<Component*> comps;
				m_currSelected->GetHasComponents(comps);

				for(unsigned int i =0; i<comps.size(); ++i)
				{
					comps[i]->GUIRender(currFrame, i);
				}
			}
		}

		ImGui::EndChild();

		ImGui::PopStyleVar();
		ImGui::End();
	}
}

void NodeController::RenderNodeTree(CGHNode* node, unsigned int uid)
{
	ImGui::PushID(uid);
	ImGui::TableNextRow();
	ImGui::TableSetColumnIndex(0);
	ImGui::AlignTextToFramePadding();

	ImGuiTreeNodeFlags flags = node == m_currSelected? ImGuiTreeNodeFlags_Selected : ImGuiTreeNodeFlags_None;
	flags |= ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;

	if(node->GetChilds().size() == 0)
	{
		flags |= ImGuiTreeNodeFlags_Leaf;
	}

	bool opned = ImGui::TreeNodeEx(node->GetName(), flags);

	if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
	{
		m_currSelected = node;
	}

	if (opned)
	{
		unsigned int index = 0;
		for (auto iter : node->GetChilds())
		{
			RenderNodeTree(iter, index++);
		}

		ImGui::TreePop();
	}

	ImGui::PopID();
}

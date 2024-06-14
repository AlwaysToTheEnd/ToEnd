#include "NodeTransformController.h"
#include "CGHNodePicker.h"
#include "../Common/Source/CGHUtil.h"
#include "imgui.h"
#include "imgui_internal.h"

NodeTransformController::NodeTransformController()
{
}

NodeTransformController::~NodeTransformController()
{
}

void NodeTransformController::Update(float delta)
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

void NodeTransformController::RenderGUI(unsigned int currFrame)
{
	if (m_active)
	{
		if (!ImGui::Begin("NodeTransformController", &m_active))
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
				RenderNodeTransform(m_currTarget, 0);
			}

			ImGui::EndTable();
		}

		ImGui::SameLine();

		if (ImGui::BeginChild("TransformInfo"))
		{
			if (m_currTarget && m_currSelected)
			{
				ImGui::SeparatorText(m_currSelected->GetName());

				auto transform = m_currSelected->GetComponent<COMTransform>();

				if (transform)
				{
					transform->GUIRender(currFrame,0);
				}
			}
		}

		ImGui::EndChild();

		ImGui::PopStyleVar();
		ImGui::End();
	}
}

void NodeTransformController::RenderNodeTransform(CGHNode* node, unsigned int uid)
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
			RenderNodeTransform(iter, index++);
		}

		ImGui::TreePop();
	}

	ImGui::PopID();
}

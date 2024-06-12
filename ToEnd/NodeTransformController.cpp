#include "NodeTransformController.h"
#include "CGHNodePicker.h"
#include "imgui.h"
#include "imgui_internal.h"

NodeTransformController::NodeTransformController()
{
}

NodeTransformController::~NodeTransformController()
{
}

void NodeTransformController::Init()
{

}

void NodeTransformController::Update(float delta)
{
	if (m_active)
	{
		auto currPicked = CGHNodePicker::s_instance.GetCurrPickedNode(CGHNODE_LAYER::CGHNODE_LYAER_CHARACTER);

		if (m_currTarget != currPicked)
		{
			m_currTarget = currPicked;
			m_currSelected = nullptr;
		}
	}
}

void NodeTransformController::RateUpdate(float delta)
{
	if (m_active)
	{

	}
}

void NodeTransformController::RenderGUI(unsigned int currFrame)
{
	if (m_active)
	{
		if (!ImGui::Begin("NodeTransformController"))
		{
			ImGui::End();
			return;
		}

		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2, 2));

		if (ImGui::BeginTable("NodeTransformControllerTable", 1, ImGuiTableFlags_RowBg |
			ImGuiTableFlags_ScrollY | ImGuiTableFlags_ScrollX))
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

		ImGui::PopStyleVar();
		ImGui::End();
	}
}

void NodeTransformController::SetSize(unsigned int x, unsigned int y)
{
}

void NodeTransformController::SetPos(unsigned int x, unsigned int y, float z)
{
}

void NodeTransformController::RenderNodeTransform(CGHNode* node, unsigned int uid)
{
	ImGui::PushID(uid);
	ImGui::TableNextRow();
	ImGui::TableSetColumnIndex(0);
	ImGui::AlignTextToFramePadding();

	ImGuiTreeNodeFlags flags = node == m_currSelected? ImGuiTreeNodeFlags_Selected : ImGuiTreeNodeFlags_None;
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

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

void NodeTransformController::Init()
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
					auto& pos = transform->GetPos();
					auto& scale = transform->GetScale();

					ImGui::DragFloat3("Pos", const_cast<float*>(&pos.x), 0.1f);
					ImGui::DragFloat3("Rot", const_cast<float*>(&m_rotation.x), 0.1f);
					ImGui::DragFloat3("Scale", const_cast<float*>(&scale.x), 0.01f);

					float piTransform = DirectX::XM_PI/ 180.0f;

					transform->SetRotateQuter(DirectX::XMQuaternionRotationRollPitchYaw(m_rotation.x * piTransform, m_rotation.y * piTransform, m_rotation.z * piTransform));
				}

			}
			ImGui::EndChild();
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
	flags |= ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;

	if(node->GetChilds().size() == 0)
	{
		flags |= ImGuiTreeNodeFlags_Leaf;
	}

	bool opned = ImGui::TreeNodeEx(node->GetName(), flags);

	if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
	{
		m_currSelected = node;

		auto transform = m_currSelected->GetComponent<COMTransform>();
		if (transform)
		{
			auto& quter = transform->GetRotationQuter();
			DirectX::XMVECTOR quterVec = DirectX::XMLoadFloat4(&quter);
			CGH::QuaternionToEulerAngles(quterVec, m_rotation.x, m_rotation.y, m_rotation.z);
		}
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

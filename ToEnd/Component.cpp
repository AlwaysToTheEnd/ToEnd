#include "Component.h"
#include "imgui.h"

void Component::GUIRender(unsigned int currFrame, unsigned int uid)
{
	ImGui::PushID(uid);
	static const char* className = "class ";
	ImGui::SeparatorText(typeid(*this).name()+ std::strlen(className));
	ImGui::SameLine();
	ImGui::Checkbox("##Active", &m_active);
	GUIRender_Internal(currFrame);
	ImGui::PopID();
}

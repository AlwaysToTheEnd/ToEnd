#include "Component.h"
#include "imgui.h"

void Component::GUIRender(unsigned int currFrame, unsigned int)
{
	static const char* className = "class ";
	ImGui::SeparatorText(typeid(*this).name()+ std::strlen(className));
	ImGui::Checkbox("Active", &m_active);
}

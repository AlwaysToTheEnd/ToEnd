#include "LightComponents.h"
#include "GraphicDeivceDX12.h"
#include "DX12GraphicResourceManager.h"
#include "imgui.h"

size_t COMDirLight::s_hashCode = typeid(COMDirLight).hash_code();
size_t COMPointLight::s_hashCode = typeid(COMPointLight).hash_code();

COMDirLight::COMDirLight(CGHNode* node)
{
}

void COMDirLight::RateUpdate(CGHNode* node, float delta)
{
	DirectX::XMVECTOR dir = { 0.0f, 0.0f, 1.0f, 0.0f };
	dir = DirectX::XMVector3Transform(dir, DirectX::XMLoadFloat4x4(&node->m_srt));

	DirectX::XMStoreFloat3(&m_data.dir, dir);
}

void COMDirLight::Render(CGHNode* node, unsigned int)
{
	auto graphic = GraphicDeviceDX12::GetGraphic();
	graphic->RenderLight(node, m_lightFlags, s_hashCode);
}

void COMDirLight::GUIRender_Internal(unsigned int currFrame)
{
	ImGui::Text("DirLight");
	ImGui::DragFloat3("Dir", &m_data.dir.x, 0.01f);
	ImGui::DragFloat3("Color", &m_data.color.x, 0.01f);
	ImGui::DragFloat("Power", &m_data.power, 0.01f);
	ImGui::CheckboxFlags("Shadow", &m_lightFlags, LIGHT_FLAG_SHADOW);
}

COMPointLight::COMPointLight(CGHNode* node)
{
}

void COMPointLight::RateUpdate(CGHNode* node, float delta)
{
	DirectX::XMVECTOR pos = { 0.0f, 0.0f, 0.0f, 1.0f };
	pos = DirectX::XMVector3Transform(pos, DirectX::XMLoadFloat4x4(&node->m_srt));
	DirectX::XMStoreFloat3(&m_data.pos, pos);
}

void COMPointLight::Render(CGHNode* node, unsigned int)
{
	auto graphic = GraphicDeviceDX12::GetGraphic();
	graphic->RenderLight(node, m_lightFlags, s_hashCode);
}
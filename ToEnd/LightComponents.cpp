#include "LightComponents.h"
#include "GraphicDeivceDX12.h"
#include "DX12GraphicResourceManager.h"

size_t COMDirLight::s_hashCode = typeid(COMDirLight).hash_code();
size_t COMPointLight::s_hashCode = typeid(COMPointLight).hash_code();

COMDirLight::COMDirLight(CGHNode* node)
{
}

void COMDirLight::RateUpdate(CGHNode* node, unsigned int, float delta)
{
	auto graphic = GraphicDeviceDX12::GetGraphic();

	DirectX::XMVECTOR dir = { 0.0f, 0.0f, 1.0f, 0.0f };
	dir = DirectX::XMVector3Transform(dir, DirectX::XMLoadFloat4x4(&node->m_srt));

	DirectX::XMStoreFloat3(&m_data.dir, dir);
	
	graphic->RenderLight(node, m_lightFlags, s_hashCode);
}

COMPointLight::COMPointLight(CGHNode* node)
{
	m_lightIndex = DX12GraphicResourceManager::s_insatance.CreateData<LightData>();
}

void COMPointLight::Release(CGHNode* ndoe)
{
	DX12GraphicResourceManager::s_insatance.ReleaseData<LightData>(m_lightIndex);
}

void COMPointLight::RateUpdate(CGHNode* node, unsigned int, float delta)
{
	auto graphic = GraphicDeviceDX12::GetGraphic();

	DirectX::XMVECTOR pos = { 0.0f, 0.0f, 0.0f, 1.0f };
	pos = DirectX::XMVector3Transform(pos, DirectX::XMLoadFloat4x4(&node->m_srt));
	DirectX::XMStoreFloat3(&m_data.pos, pos);

	DX12GraphicResourceManager::s_insatance.SetData<LightData>(m_lightIndex, &m_data);
	graphic->RenderLight(node, m_lightFlags, s_hashCode);
}

UINT64 COMPointLight::GetLightDataGPU(unsigned int currFrameIndex)
{
	return DX12GraphicResourceManager::s_insatance.GetGpuAddress<LightData>(m_lightIndex, currFrameIndex);
}

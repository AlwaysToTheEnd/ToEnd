#include "LightComponents.h"
#include "GraphicDeivceDX12.h"
#include "DX12GraphicResourceManager.h"

size_t COMDirLight::s_hashCode = typeid(COMDirLight).hash_code();

COMDirLight::COMDirLight(CGHNode* node)
{
	m_lightIndex = DX12GraphicResourceManager::s_insatance.CreateData<LightData>();
}

void COMDirLight::RateUpdate(CGHNode* node, unsigned int, float delta)
{
	auto graphic = GraphicDeviceDX12::GetGraphic();

	DX12GraphicResourceManager::s_insatance.SetData<LightData>(m_lightIndex, &m_data);
	graphic->RenderLight(node, m_lightFlags, s_hashCode);
}

UINT64 COMDirLight::GetLightDataGPU(unsigned int currFrameIndex)
{
	return DX12GraphicResourceManager::s_insatance.GetGpuAddress<LightData>(m_lightIndex, currFrameIndex);
}

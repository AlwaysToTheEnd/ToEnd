#include "LightComponents.h"
#include "GraphicDeivceDX12.h"

size_t COMDirLight::s_hashCode = typeid(COMDirLight).hash_code();

COMDirLight::COMDirLight(CGHNode* node)
{

}

void COMDirLight::RateUpdate(CGHNode* node, unsigned int, float delta)
{
	auto graphic = GraphicDeviceDX12::GetGraphic();

	graphic->RenderLight(node, m_lightFlags, 0);
}

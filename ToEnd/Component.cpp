#include "Component.h"
#include "CGHBaseClass.h"
#include <typeinfo>


size_t Transform::s_hashCode = typeid(Transform).hash_code();

Transform::Transform(CGHNode* node)
{
	m_node = node;
}

void Transform::Update(float delta)
{
	DirectX::XMMATRIX rotateMat = DirectX::XMMatrixRotationQuaternion(DirectX::XMLoadFloat4(&m_queternion));
	DirectX::XMMATRIX scaleMat = DirectX::XMMatrixScaling(m_scale.x, m_scale.y, m_scale.z);
	DirectX::XMMATRIX transMat = DirectX::XMMatrixTranslation(m_pos.x, m_pos.y, m_pos.z); 

	CGHNode* parentNode = m_node->GetParent();
	if (parentNode != nullptr)
	{
		DirectX::XMStoreFloat4x4(&m_node->m_srt, scaleMat * rotateMat * transMat * DirectX::XMLoadFloat4x4(&parentNode->m_srt));
	}
	else
	{
		DirectX::XMStoreFloat4x4(&m_node->m_srt, scaleMat * rotateMat * transMat);
	}
}

void Transform::RateUpdate(float delta)
{

}

size_t Transform::GetTypeHashCode()
{
	return s_hashCode;
}

void XM_CALLCONV Transform::SetPos(DirectX::FXMVECTOR pos)
{
	DirectX::XMStoreFloat3(&m_pos, pos);
}

void XM_CALLCONV Transform::SetScale(DirectX::FXMVECTOR scale)
{
	DirectX::XMStoreFloat3(&m_scale, scale);
}

void XM_CALLCONV Transform::SetRotateQuter(DirectX::FXMVECTOR quterRotate)
{
	DirectX::XMStoreFloat4(&m_queternion, quterRotate);
}

#include "Component.h"
#include <typeinfo>


size_t Transform::s_hashCode = typeid(Transform).hash_code();





Transform::Transform(CGHNode* node)
{
	m_node = node;
}

void Transform::Update(float delta)
{
}

void Transform::RateUpdate(float delta)
{

}

size_t Transform::GetTypeHashCode()
{
	return s_hashCode;
}

#include "Component.h"
#include <typeinfo>
size_t Transform::s_hashCode = typeid(Transform).hash_code();


Transform::Transform(CGHNode* node)
{
	m_node = node;
}

size_t Transform::GetTypeHashCode()
{
	return s_hashCode;
}

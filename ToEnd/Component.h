#pragma once
#include <string>
#include <DirectXMath.h>

class CGHNode;

class Component
{
public:
	virtual void Update(float delta) = 0;
	virtual void RateUpdate(float delta) = 0;
	virtual size_t GetTypeHashCode() = 0;
};


class Transform : public Component
{
public:
	Transform(CGHNode* node);

	virtual void Update(float delta);
	virtual void RateUpdate(float delta);
	virtual size_t GetTypeHashCode();

private:
	static size_t s_hashCode;
	CGHNode* m_node;
	
};
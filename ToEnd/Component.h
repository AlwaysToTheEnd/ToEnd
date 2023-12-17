#pragma once
#include <string>
#include <vector>
#include <DirectXMath.h>

class CGHNode;

class Component
{
public:
	virtual void Update(float delta) = 0;
	virtual void RateUpdate(float delta) = 0;
	virtual size_t GetTypeHashCode() = 0;
	virtual unsigned int GetPriority() = 0;

protected:
	CGHNode* m_node;
};


class Transform : public Component
{
public:
	Transform(CGHNode* node);

	virtual void Update(float delta) override;
	virtual void RateUpdate(float delta) override;
	virtual size_t GetTypeHashCode() override;
	virtual unsigned int GetPriority() override { return 0; }

private:
	static size_t s_hashCode;
	
};
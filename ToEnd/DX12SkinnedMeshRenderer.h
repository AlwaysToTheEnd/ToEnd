#pragma once
#include <d3d12.h>
#include <memory>
#include "DX12UploadBuffer.h"
#include "Component.h"
struct CGHMeshDataSet;

class COMDX12SkinnedMeshRenderer : public Component
{
public:
	COMDX12SkinnedMeshRenderer();
	~COMDX12SkinnedMeshRenderer() = default;

	virtual void Update(CGHNode* node, float delta) override {}
	virtual void RateUpdate(CGHNode* node, float delta) override;
	virtual size_t GetTypeHashCode() override { return s_hashCode; }
	virtual unsigned int GetPriority() override { return COMPONENT_SKINNEDMESH_RENDERER; }

private:
	static size_t s_hashCode;
};


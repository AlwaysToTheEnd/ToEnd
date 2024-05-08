#pragma once
#include <d3d12.h>
#include <functional>

struct PipeLineWorkSet
{
	int psoType = 0;
	ID3D12PipelineState* pso = nullptr;
	ID3D12RootSignature* rootSig = nullptr;
	std::function<void(ID3D12GraphicsCommandList* cmd)> baseGraphicCmdFunc;
	std::function<void(ID3D12GraphicsCommandList* cmd, CGHNode* node)> nodeGraphicCmdFunc;
};
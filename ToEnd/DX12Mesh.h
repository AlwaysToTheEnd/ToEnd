#pragma once
#include <string>
#include <memory>
#include <wrl.h>
#include <d3d12.h>
#include <DirectXMath.h>

using Microsoft::WRL::ComPtr;


class DX12Mesh
{
public:
	DX12Mesh();
	~DX12Mesh();

	void Render(ID3D12GraphicsCommandList* cmd);

	void SetVertexBuffer(ComPtr<ID3D12Resource> vertexBuffer);
	void SetIndexBuffer(ComPtr<ID3D12Resource> indexBuffer);
	void SetGraphicPipeLine(ComPtr<ID3D12PipelineState> pipelineState);

private:
	ComPtr<ID3D12Resource> m_vertexBuffer;
	ComPtr<ID3D12Resource> m_indexBuffer;
	ComPtr<ID3D12PipelineState> m_currPipeLine;
};


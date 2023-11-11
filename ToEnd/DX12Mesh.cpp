#include "DX12Mesh.h"
#include "DX12PipelineMG.h"
#include "GraphicDeivceDX12.h"

DX12Mesh::DX12Mesh()
{

}

DX12Mesh::~DX12Mesh()
{
	if (m_vertexBuffer)
	{
		m_vertexBuffer->Release();
	}

	if (m_indexBuffer)
	{
		m_indexBuffer->Release();
	}
}

void DX12Mesh::Render(ID3D12GraphicsCommandList* cmd)
{

}

void DX12Mesh::SetVertexBuffer(ComPtr<ID3D12Resource> vertexBuffer)
{
	if (vertexBuffer == m_vertexBuffer)
	{
		return;
	}

	if (m_vertexBuffer !=nullptr)
	{
		m_vertexBuffer->Release();
	}

	m_vertexBuffer = vertexBuffer;
	m_vertexBuffer->AddRef();
}

void DX12Mesh::SetIndexBuffer(ComPtr<ID3D12Resource> indexBuffer)
{
	if (indexBuffer == m_indexBuffer)
	{
		return;
	}

	if (m_indexBuffer != nullptr)
	{
		m_indexBuffer->Release();
	}

	m_indexBuffer = indexBuffer;
	m_indexBuffer->AddRef();
}

void DX12Mesh::SetGraphicPipeLine(ComPtr<ID3D12PipelineState> pipelineState)
{
	m_currPipeLine = pipelineState;
}


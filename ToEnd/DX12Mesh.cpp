#include "DX12Mesh.h"
#include "DX12PipelineMG.h"
#include "GraphicDeivceDX12.h"

void* DX12Mesh::s_meshPipelinestate = nullptr;
void* DX12Mesh::s_commandSignature = nullptr;

void DX12Mesh::DX12MeshBaseDataInit()
{
	D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineDesc;

	D3D12_COMMAND_SIGNATURE_DESC commandSignatureDesc = {};
	commandSignatureDesc.pArgumentDescs = s_normalMeshIndArgDesc;
	commandSignatureDesc.NumArgumentDescs = _countof(s_normalMeshIndArgDesc);
	commandSignatureDesc.ByteStride = sizeof(DX12NormalMeshIndirectCommand);

	s_meshPipelinestate = s_pipelineMG.CreateGraphicPipeline("normalMesh", &pipelineDesc);
	s_commandSignature = s_pipelineMG.CreateCommandSignature("nomalMehs", , &commandSignatureDesc);
}

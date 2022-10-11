#include "DX12PipelineMG.h"
#include <d3dcompiler.h>
#include <assert.h>
#include "GraphicDeivceDX12.h"
#include "../Common/Source/DxException.h"
#pragma comment(lib,"d3dcompiler.lib")

using Microsoft::WRL::ComPtr;

DX12PipelineMG::DX12PipelineMG()
{
}

DX12PipelineMG::~DX12PipelineMG()
{
}

D3D12_SHADER_BYTECODE DX12PipelineMG::CreateShader(DX12_SHADER_TYPE type, const char* shaderName,
													const wchar_t* filename, const std::string& entrypoint, const D3D_SHADER_MACRO* defines)
{
	std::string target;

	switch (type)
	{
	case DX12_SHADER_VERTEX:
		target = "vs";
		break;
	case DX12_SHADER_PIXEL:
		target = "ps";
		break;
	case DX12_SHADER_GEOMETRY:
		target = "gs";
		break;
	case DX12_SHADER_HULL:
		target = "hs";
		break;
	case DX12_SHADER_DOMAIN:
		target = "ds";
		break;
	case DX12_SHADER_COMPUTE:
		target = "cs";
		break;
	case DX12_SHADER_TYPE_COUNT:
	default:
		assert(false);
		break;
	}

	target += m_shaderVersion;

	auto iter = m_shaders[type].find(shaderName);
	assert(iter == m_shaders[type].end());

	UINT compileFlags = 0;
#if defined(DEBUG) || defined(_DEBUG)  
	compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

	HRESULT hr = S_OK;

	ComPtr<ID3DBlob> byteCode = nullptr;
	ComPtr<ID3DBlob> errors;
	hr = D3DCompileFromFile(filename, defines, D3D_COMPILE_STANDARD_FILE_INCLUDE,
		entrypoint.c_str(), target.c_str(), compileFlags, 0, &byteCode, &errors);

	if (errors != nullptr)
	{
		OutputDebugStringA((char*)errors->GetBufferPointer());
	}

	ThrowIfFailed(hr);

	m_shaders[type].insert({ shaderName, byteCode });

	return D3D12_SHADER_BYTECODE({ byteCode->GetBufferPointer(), byteCode->GetBufferSize()});
}

ID3D12PipelineState* DX12PipelineMG::CreateGraphicPipeline(const char* name, const D3D12_GRAPHICS_PIPELINE_STATE_DESC* desc)
{
	auto GraphicDevice = GraphicDeviceDX12::GetDevice();

	auto iter = m_pipeLines.find(name);
	assert(iter == m_pipeLines.end());

	ThrowIfFailed(GraphicDevice->CreateGraphicsPipelineState(desc, IID_PPV_ARGS(m_pipeLines[name].GetAddressOf())));

	return m_pipeLines[name].Get();
}

ID3D12CommandSignature* DX12PipelineMG::CreateCommandSignature(const char* name, ID3D12RootSignature* rootsignature, const D3D12_COMMAND_SIGNATURE_DESC* desc)
{
	auto GraphicDevice = GraphicDeviceDX12::GetDevice();

	auto iter = m_commandSignatures.find(name);
	assert(iter == m_commandSignatures.end());

	ThrowIfFailed(GraphicDevice->CreateCommandSignature(desc, rootsignature, IID_PPV_ARGS(m_commandSignatures[name].GetAddressOf())));

	return m_commandSignatures[name].Get();
}

D3D12_SHADER_BYTECODE DX12PipelineMG::GetShader(DX12_SHADER_TYPE type, const char* shaderName)
{
	D3D12_SHADER_BYTECODE result = {};

	auto iter = m_shaders[type].find(shaderName);
	assert(iter != m_shaders[type].end());

	result.BytecodeLength = iter->second->GetBufferSize();
	result.pShaderBytecode = iter->second->GetBufferPointer();

	return result;
}

ID3D12PipelineState* DX12PipelineMG::GetGraphicPipeline(const char* name)
{
	ID3D12PipelineState* result = nullptr;

	auto iter = m_pipeLines.find(name);
	assert(iter != m_pipeLines.end());

	result = iter->second.Get();

	return result;
}

ID3D12CommandSignature* DX12PipelineMG::GetCommandSignature(const char* name)
{
	ID3D12CommandSignature* result = nullptr;

	auto iter = m_commandSignatures.find(name);
	assert(iter != m_commandSignatures.end());

	result = iter->second.Get();

	return result;
}

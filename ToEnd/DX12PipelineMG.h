#pragma once
#include <d3d12.h>
#include <DirectXMath.h>
#include <wrl.h>
#include <string>
#include <unordered_map>
#include "d3dx12.h"

static D3D12_STATIC_SAMPLER_DESC s_StaticSamplers[7] =
{
	CD3DX12_STATIC_SAMPLER_DESC(
	0,
	D3D12_FILTER_MIN_MAG_MIP_POINT,
	D3D12_TEXTURE_ADDRESS_MODE_WRAP,
	D3D12_TEXTURE_ADDRESS_MODE_WRAP,
	D3D12_TEXTURE_ADDRESS_MODE_WRAP),

	CD3DX12_STATIC_SAMPLER_DESC(
	1,
	D3D12_FILTER_MIN_MAG_MIP_POINT,
	D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
	D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
	D3D12_TEXTURE_ADDRESS_MODE_CLAMP),

	CD3DX12_STATIC_SAMPLER_DESC(
	2,
	D3D12_FILTER_MIN_MAG_MIP_LINEAR,
	D3D12_TEXTURE_ADDRESS_MODE_WRAP,
	D3D12_TEXTURE_ADDRESS_MODE_WRAP,
	D3D12_TEXTURE_ADDRESS_MODE_WRAP),

	CD3DX12_STATIC_SAMPLER_DESC(
	3,
	D3D12_FILTER_MIN_MAG_MIP_LINEAR,
	D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
	D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
	D3D12_TEXTURE_ADDRESS_MODE_CLAMP),

	CD3DX12_STATIC_SAMPLER_DESC(
	4,
	D3D12_FILTER_ANISOTROPIC,
	D3D12_TEXTURE_ADDRESS_MODE_WRAP,
	D3D12_TEXTURE_ADDRESS_MODE_WRAP,
	D3D12_TEXTURE_ADDRESS_MODE_WRAP,
	0.0f,
	8),

	CD3DX12_STATIC_SAMPLER_DESC(
	5,
	D3D12_FILTER_ANISOTROPIC,
	D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
	D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
	D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
	0.0f,
	8),

	 CD3DX12_STATIC_SAMPLER_DESC(
	6,
	D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT,
	D3D12_TEXTURE_ADDRESS_MODE_BORDER,
	D3D12_TEXTURE_ADDRESS_MODE_BORDER,
	D3D12_TEXTURE_ADDRESS_MODE_BORDER,
	0.0f,
	16,
	D3D12_COMPARISON_FUNC_LESS_EQUAL,
	D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK)
};

enum DX12_SHADER_TYPE
{
	DX12_SHADER_VERTEX,
	DX12_SHADER_PIXEL,
	DX12_SHADER_GEOMETRY,
	DX12_SHADER_HULL,
	DX12_SHADER_DOMAIN,
	DX12_SHADER_COMPUTE,
	DX12_SHADER_TYPE_COUNT
};

static class DX12PipelineMG
{
private:
	~DX12PipelineMG();
	DX12PipelineMG();
public:
	static DX12PipelineMG instance;

	D3D12_SHADER_BYTECODE	CreateShader(DX12_SHADER_TYPE type, const char* shaderName, const wchar_t* filename,
										const std::string& entrypoint, const D3D_SHADER_MACRO* defines=nullptr);
	ID3D12PipelineState*	CreateGraphicPipeline(const char* name, const D3D12_GRAPHICS_PIPELINE_STATE_DESC* desc);
	ID3D12RootSignature*	CreateRootSignature(const char* name, const D3D12_ROOT_SIGNATURE_DESC* desc);
	ID3D12CommandSignature* CreateCommandSignature(const char* name, ID3D12RootSignature* rootsignature, const D3D12_COMMAND_SIGNATURE_DESC* desc);

	D3D12_SHADER_BYTECODE	GetShader(DX12_SHADER_TYPE type, const char* shaderName);
	ID3D12PipelineState*	GetGraphicPipeline(const char* name);
	ID3D12RootSignature*	GetRootSignature(const char* name);
	ID3D12CommandSignature* GetCommandSignature(const char* name);

private:
	const char* m_shaderVersion = "_5_1";

	std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D12PipelineState>>	m_pipeLines;
	std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D12RootSignature>>	m_rootSignatures;
	std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D12CommandSignature>>	m_commandSignatures;
	std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3DBlob>>				m_shaders[DX12_SHADER_TYPE_COUNT];

};


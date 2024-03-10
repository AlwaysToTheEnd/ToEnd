#pragma once
#include <d3d12.h>
#include <string>

struct DX12_PIPELINE_IASET
{
	D3D12_INPUT_LAYOUT_DESC inputLayout;
	D3D12_PRIMITIVE_TOPOLOGY_TYPE primitive;
	D3D12_INDEX_BUFFER_STRIP_CUT_VALUE ibStripcutValue;
};

struct DX12_GRAPHICS_PIPELINE_DEFINE
{
	std::string psoName;

	std::string vs_name;
	std::string gs_name;
	std::string ds_name;
	std::string hs_name;
	std::string ps_name;

	const char* sigName;

	const char* iaSet;
	const char* omSet;
	const char* blend;
	const char* resterizer;
};
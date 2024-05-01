#pragma once
#include <d3d12.h>
#include <vector>
#include <wrl.h>
#include <dxgiformat.h>

class DX12SMAA
{
	enum TEXTURE_TYPE
	{
		TEXTYPE_SEARCHTH,
		TEXTYPE_AREA,
		TEXTYPE_EDGES,
		TEXTYPE_BLEND,
		TEXTYPE_COLOR,
		TEXTYPE_NUM
	};

	const DXGI_FORMAT m_textureFormats[TEXTYPE_NUM] = 
	{ DXGI_FORMAT_R8_UNORM, DXGI_FORMAT_R8G8_UNORM,
		DXGI_FORMAT_R8G8_UNORM, DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_R8G8B8A8_UNORM };
public:
	DX12SMAA() = default;
	~DX12SMAA() = default;

	ID3D12DescriptorHeap* GetSRVHeap();
	D3D12_CPU_DESCRIPTOR_HANDLE GetEdgeRenderTarget();
	D3D12_CPU_DESCRIPTOR_HANDLE GetBlendRenderTarget();
	D3D12_CPU_DESCRIPTOR_HANDLE GetColorRenderTarget();
	std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> Resize(ID3D12Device* device, ID3D12GraphicsCommandList* cmd, unsigned int winX, unsigned int winY);

private:
	unsigned int m_rtvSize = 0;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_srvHeaps;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_rtvHeaps;

	Microsoft::WRL::ComPtr<ID3D12Resource> m_textures[TEXTYPE_NUM] = {};
};


#pragma once
#include <unordered_map>
#include <string>
#include <d3d12.h>
#include <wrl.h>
#include "DX12UploadBuffer.h"
#include "../Common/Source/CGHUtil.h"

class CGHFontLoader;

struct DX12FontTriangles
{
	uint16_t advance_width = 0;
	int16_t left_side_bearing = 0;
	int16_t bounding_box[4] = {};
	DirectX::XMFLOAT2 glyph_center = { 0,0 };
	unsigned int numVertex = 0;
	unsigned int vertexOffset = 0;
	unsigned int numIndex = 0;
	unsigned int indexOffset = 0;
};

struct DX12Font
{
	Microsoft::WRL::ComPtr<ID3D12Resource> vertices;
	Microsoft::WRL::ComPtr<ID3D12Resource> indices;
	Microsoft::WRL::ComPtr<ID3D12Resource> fontInfos;
	std::unordered_map<unsigned int, DX12FontTriangles> glyphInfos;
};

struct DX12FontText
{
	std::string text;
	unsigned int fontSize = 1;
	DirectX::XMFLOAT3 pos;
};

static class DX12FontManager
{
public:
	DX12FontManager();
	~DX12FontManager();

	DX12Font* GetFont(const char* filePath);

private:
	DX12Font* CreateDX12Font(const char* filePath);

private:
	std::unordered_map<std::string, DX12Font> m_fonts;
	ID3D12Device* m_device = nullptr;
	CGHFontLoader* m_fontLoader = nullptr;
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> m_cmd;
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> m_alloc;
	Microsoft::WRL::ComPtr<ID3D12Fence> m_fence;
	UINT64 m_fenceValue = 0;

}s_dx12FontMG;


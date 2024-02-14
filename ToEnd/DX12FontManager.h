#pragma once
#include <unordered_map>
#include <string>
#include <d3d12.h>
#include <wrl.h>
#include "DX12UploadBuffer.h"
#include "../Common/Source/CGHUtil.h"

class CGHFontLoader;

struct DX12FontMeshInfo
{
	UINT advance_width = 0;
	UINT left_side_bearing = 0;
	DirectX::XMFLOAT2 glyph_center = { 0,0 };
	UINT bounding_box[4] = {};
	unsigned int numVertex = 0;
	unsigned int vertexOffset = 0;
	unsigned int numIndex = 0;
	unsigned int indexOffset = 0;
};

struct DX12Font
{
	Microsoft::WRL::ComPtr<ID3D12Resource> vertices;
	Microsoft::WRL::ComPtr<ID3D12Resource> indices;
	Microsoft::WRL::ComPtr<ID3D12Resource> fontMeshInfos;
	std::vector<DX12FontMeshInfo> cpuFontMeshInfos;
	uint16_t infoIndex[UINT16_MAX] = {};
};

struct DX12FontText
{
	std::string text;
	unsigned int fontSize = 1;
	DirectX::XMFLOAT3 pos;
};

class DX12FontManager
{
public:
	static DX12FontManager instance;
	const DX12Font* GetFont(const char* filePath);

private:
	DX12FontManager();
	~DX12FontManager();
	DX12Font* CreateDX12Font(const char* filePath);


private:
	std::unordered_map<std::string, DX12Font> m_fonts;
	CGHFontLoader* m_fontLoader = nullptr;
};


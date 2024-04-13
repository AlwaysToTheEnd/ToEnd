#pragma once
#include <Windows.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <d3d12.h>
#include <wrl.h>

class DX12FontManger
{
	struct Glyph
	{
		uint32_t character;
		RECT subrect;
		float xOffset;
		float yOffset;
		float xAdvance;
	};

	struct DX12Font
	{
		float lineSpacing = 0;
		uint32_t defaultChar = 0;
		std::unordered_map<uint32_t, Glyph> glyphs;
		Microsoft::WRL::ComPtr<ID3D12Resource> texture;
	};
public:
	static const DX12Font* LoadFont(const wchar_t* filePath);

private:
	void CreateFontData(const wchar_t* filePath);


private:
	static DX12FontManger s_instance;

	const std::string m_fontToken = "DXTKfont";
	std::unordered_map<std::wstring, DX12Font> m_fonts;
};
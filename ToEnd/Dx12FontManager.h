#pragma once
#include <Windows.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <d3d12.h>
#include <DirectXMath.h>
#include <wrl.h>
#include <assert.h>

namespace CGH
{
	struct FontGlyph
	{
		uint32_t character;
		RECT subrect_left;
		float xOffset;
		float yOffset;
		float xAdvance;
	};

	struct DX12Font
	{
		struct CharSubCategory
		{
			uint16_t indexOffset = 0;
			uint16_t charCodeStart = 0;
			uint16_t charNum = UINT16_MAX;
		};

		unsigned int GetGlyphIndex(uint32_t charCode)
		{
			unsigned int result = 0;

			assert(charCode <= UINT16_MAX);
			for (auto& iter : category)
			{
				if (iter.charCodeStart >= charCode && charCode < iter.charNum + iter.charCodeStart)
				{
					result = charCode - iter.charCodeStart + iter.indexOffset;
					break;
				}
			}

			return result;
		}

		float lineSpacing = 0;
		uint32_t defaultChar = 0;
		uint32_t textureWidth = 0;
		uint32_t textureHeight = 0;
		std::vector<FontGlyph> glyphs;
		std::vector<CharSubCategory> category;
		Microsoft::WRL::ComPtr<ID3D12Resource> texture;
		Microsoft::WRL::ComPtr<ID3D12Resource> glyphDatas;
	};
}

class DX12FontManger
{
	struct CharInfo
	{
		float xOffsetInString;
		uint32_t stringIndex;
		uint32_t glyphID;
		uint32_t stringInfoID;
	};

	struct StringInfo
	{
		DirectX::XMFLOAT4 color;
		DirectX::XMFLOAT3 pos;
		float size = 1.0f;
		uint32_t renderID = UINT32_MAX;
		float pad[3] = {};
	};
public:
	void Init();
	const CGH::DX12Font* LoadFont(const wchar_t* filePath);

private:
	DX12FontManger() = default;
	~DX12FontManger() = default;
	void CreateFontData(const wchar_t* filePath);

public:
	static DX12FontManger s_instance;

private:
	const unsigned int								m_maxNumChar = 4096;
	const unsigned int								m_maxNumString = 1024;
	const std::string								m_fontToken = "DXTKfont";
	CGH::DX12Font*									m_currfont = nullptr;
	std::unordered_map<std::wstring, CGH::DX12Font> m_fonts;

	unsigned int									m_numRenderChar = 0;
	Microsoft::WRL::ComPtr<ID3D12Resource>			m_stringInfos;
	Microsoft::WRL::ComPtr<ID3D12Resource>			m_charInfos;
};
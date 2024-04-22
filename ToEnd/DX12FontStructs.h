#pragma once
#include <d3d12.h>
#include <windef.h>
#include <DirectXMath.h>
#include <wrl.h>

namespace CGH
{
	struct FontGlyph
	{
		uint32_t character;
		RECT subrect;
		float xOffset;
		float yOffset;
		float xAdvance;
	};

	struct CharInfo
	{
		DirectX::XMFLOAT4 color;
		DirectX::XMFLOAT2 leftTopP;
		DirectX::XMFLOAT2 rightBottomP;
		float depth = 0;
		uint32_t glyphID = 0;
		float pad0 = 0;
		float pad1 = 0;
	};

	struct DX12Font
	{
		struct CharSubCategory
		{
			uint16_t indexOffset = 0;
			uint16_t charCodeStart = 0;
			uint16_t charNum = UINT16_MAX;
		};

		struct GlyphForDX12
		{
			DirectX::XMFLOAT2 uvLT = {};
			DirectX::XMFLOAT2 uvRB = {};
		};

		unsigned int GetGlyphIndex(uint32_t charCode) const
		{
			unsigned int result = 0;

			assert(charCode <= UINT16_MAX);
			for (auto& iter : category)
			{
				if (charCode >= iter.charCodeStart && charCode < iter.charNum + iter.charCodeStart)
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
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> textureHeap;
		Microsoft::WRL::ComPtr<ID3D12Resource> glyphDatas;
	};
}
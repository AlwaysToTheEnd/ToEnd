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
		uint32_t renderID = 0;
		float pad0 = 0;
	};

	struct DX12Font;

	class DX12RenderString
	{
	public:
		DX12RenderString(unsigned int index)
			:m_Index(index)
		{
		}
		~DX12RenderString();

		void SetRenderString(const wchar_t* str,
			const DirectX::XMFLOAT4& color, const DirectX::XMFLOAT3& pos, float size, float rowPitch, uint32_t renderID);
		void ReroadDataFromCurrFont();
		bool GetActive() { return m_isActive; }
		unsigned int GetStringSize() { return m_charInfos.size(); }
		const CGH::CharInfo* GetCharInfoDatas() { return m_charInfos.data(); }

		void SetActive(bool active) { m_isActive = active; }
		void SetIndex(unsigned int index) { m_Index = index; }

	private:
		std::wstring				m_string;
		DirectX::XMFLOAT3			m_pos = {};
		float						m_rowPitch = 100.0f;
		float						m_size = 1.0f;
		bool						m_isActive = true;
		unsigned int				m_Index = 0;
		std::vector<CGH::CharInfo>	m_charInfos;
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
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> textureHeap;
		Microsoft::WRL::ComPtr<ID3D12Resource> glyphDatas;
	};
}

class DX12FontManger
{
public:
	void Init();
	void AllStringFontReroad();

	const CGH::DX12Font* LoadFont(const wchar_t* filePath);
	const CGH::DX12Font* GetCurrFont() { return m_currfont; }

	CGH::DX12RenderString* CreateRenderString();
	void ReleaseRenderString(unsigned int index);

private:
	DX12FontManger() = default;
	~DX12FontManger();
	CGH::DX12Font* CreateFontData(const wchar_t* filePath);

public:
	static DX12FontManger s_instance;

private:
	const unsigned int								m_maxNumChar = 4096;
	const std::string								m_fontToken = "DXTKfont";
	CGH::DX12Font*									m_currfont = nullptr;
	std::unordered_map<std::wstring, CGH::DX12Font> m_fonts;

	Microsoft::WRL::ComPtr<ID3D12Resource>	m_charInfos;
	std::vector<CGH::CharInfo*>				m_charInfoMapped;
	std::vector<CGH::DX12RenderString*>		m_renderStrings;
};
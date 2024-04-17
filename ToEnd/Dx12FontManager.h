#pragma once
#include <Windows.h>
#include <unordered_map>
#include <assert.h>
#include "DX12FontStructs.h"

class DX12FontManger
{
public:
	void Init();

	void RenderString(CGH::DX12RenderString& str, unsigned int currFrame);
	const CGH::DX12Font* LoadFont(const wchar_t* filePath);
	const CGH::DX12Font* GetCurrFont() { return m_currfont; }

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
	unsigned int							m_numRenderChar = 0;
};
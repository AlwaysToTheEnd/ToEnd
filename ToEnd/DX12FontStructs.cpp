#include "DX12FontStructs.h"
#include "Dx12FontManager.h"
#include "CGHBaseClass.h"

using namespace DirectX;

void CGH::DX12RenderString::SetRenderString(const wchar_t* _str,
	DirectX::FXMVECTOR _color, const DirectX::XMFLOAT3& _pos, float _scale, float _rowPitch, uint32_t renderID)
{
	str = _str;
	DirectX::XMStoreFloat4(&color, _color);
	charInfos.resize(str.length());

	{
		currFont = DX12FontManger::s_instance.GetCurrFont();
		const auto& glyphs = currFont->glyphs;
		XMFLOAT2 winSizeReciprocal;
		winSizeReciprocal.x = 1.0f / GO.WIN.WindowsizeX;
		winSizeReciprocal.y = 1.0f / GO.WIN.WindowsizeY;

		size_t index = 0;
		float offsetInString = 0.0f;
		int currLine = 0;
		for (auto& iter : charInfos)
		{
			iter.glyphID = currFont->GetGlyphIndex(str[index]);
			auto& currGlyph = glyphs[iter.glyphID];
			XMFLOAT2 fontSize = { float(currGlyph.subrect.right - currGlyph.subrect.left) * _scale,float(currGlyph.subrect.bottom - currGlyph.subrect.top) * _scale };
			iter.depth = _pos.z;
			iter.renderID = renderID;
			iter.color = color;
			iter.leftTopP = { _pos.x + offsetInString, _pos.y + (currGlyph.yOffset + (currFont->lineSpacing * currLine)) * _scale };
			iter.rightBottomP = { iter.leftTopP.x + fontSize.x, iter.leftTopP.y + fontSize.y };

			iter.leftTopP.x = iter.leftTopP.x * winSizeReciprocal.x * 2.0f - 1.0f;
			iter.leftTopP.y = 1.0f - (iter.leftTopP.y * winSizeReciprocal.y * 2.0f);
			iter.rightBottomP.x = iter.rightBottomP.x * winSizeReciprocal.x * 2.0f - 1.0f;
			iter.rightBottomP.y = 1.0f - (iter.rightBottomP.y * winSizeReciprocal.y * 2.0f);

			if (offsetInString + fontSize.x > _rowPitch)
			{
				offsetInString = 0;
				currLine++;
			}
			else
			{
				offsetInString += (float(currGlyph.subrect.right) - float(currGlyph.subrect.left) + currGlyph.xAdvance + currGlyph.xOffset) * _scale;
			}

			index++;
		}
	}

	pos = _pos;
	rowPitch = _rowPitch;
	scaleSize = _scale;
}

void CGH::DX12RenderString::ReroadDataFromCurrFont()
{
	if (charInfos.size())
	{
		auto currFont = DX12FontManger::s_instance.GetCurrFont();
		const auto& glyphs = currFont->glyphs;
		XMFLOAT2 winSizeReciprocal;
		winSizeReciprocal.x = 1.0f / GO.WIN.WindowsizeX;
		winSizeReciprocal.y = 1.0f / GO.WIN.WindowsizeY;

		XMFLOAT2 sizeInP = { winSizeReciprocal.x * 2.0f * scaleSize, winSizeReciprocal.y * 2.0f * scaleSize };
		unsigned int renderID = charInfos.front().renderID;

		charInfos.resize(str.length());
		size_t index = 0;
		float offsetInString = 0.0f;
		for (auto& iter : charInfos)
		{
			iter.glyphID = currFont->GetGlyphIndex(str[index]);
			auto& currGlyph = glyphs[iter.glyphID];
			XMFLOAT2 fontSize = { (currGlyph.subrect.right - currGlyph.subrect.right) * sizeInP.x,(currGlyph.subrect.bottom - currGlyph.subrect.top) * sizeInP.y };

			iter.depth = pos.z;
			iter.renderID = renderID;
			iter.color = color;
			iter.leftTopP = { pos.x + offsetInString, pos.y };
			iter.rightBottomP = { iter.leftTopP.x + fontSize.x, iter.leftTopP.y - fontSize.y };

			iter.leftTopP.x = iter.leftTopP.x * winSizeReciprocal.x * 2.0f - 1.0f;
			iter.leftTopP.y = iter.leftTopP.y * winSizeReciprocal.y * 2.0f - 1.0f;
			iter.rightBottomP.x = iter.rightBottomP.x * winSizeReciprocal.x * 2.0f - 1.0f;
			iter.rightBottomP.y = iter.rightBottomP.y * winSizeReciprocal.y * 2.0f - 1.0f;

			offsetInString += fontSize.x + (currGlyph.xAdvance * fontSize.x / 100);
			index++;
		}

		currFont = currFont;
	}
}

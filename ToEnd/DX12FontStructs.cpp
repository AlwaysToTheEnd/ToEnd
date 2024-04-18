#include "DX12FontStructs.h"
#include "Dx12FontManager.h"
#include "CGHBaseClass.h"

using namespace DirectX;

void CGH::DX12RenderString::SetRenderString(const wchar_t* _str,
	DirectX::FXMVECTOR _color, const DirectX::XMFLOAT3& _pos, float _scale, float _rowPitch)
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
			iter.color = color;
			offsetInString += currGlyph.xOffset * _scale;
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
				offsetInString += (float(currGlyph.subrect.right) - float(currGlyph.subrect.left) + currGlyph.xAdvance) * _scale;
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
			XMFLOAT2 fontSize = { float(currGlyph.subrect.right - currGlyph.subrect.left) * scaleSize,float(currGlyph.subrect.bottom - currGlyph.subrect.top) * scaleSize };
			iter.depth = pos.z;
			iter.color = color;
			iter.leftTopP = { pos.x + offsetInString, pos.y + (currGlyph.yOffset + (currFont->lineSpacing * currLine)) * scaleSize };
			iter.rightBottomP = { iter.leftTopP.x + fontSize.x, iter.leftTopP.y + fontSize.y };

			iter.leftTopP.x = iter.leftTopP.x * winSizeReciprocal.x * 2.0f - 1.0f;
			iter.leftTopP.y = 1.0f - (iter.leftTopP.y * winSizeReciprocal.y * 2.0f);
			iter.rightBottomP.x = iter.rightBottomP.x * winSizeReciprocal.x * 2.0f - 1.0f;
			iter.rightBottomP.y = 1.0f - (iter.rightBottomP.y * winSizeReciprocal.y * 2.0f);

			if (offsetInString + fontSize.x > rowPitch)
			{
				offsetInString = 0;
				currLine++;
			}
			else
			{
				offsetInString += (float(currGlyph.subrect.right) - float(currGlyph.subrect.left) + currGlyph.xAdvance + currGlyph.xOffset) * scaleSize;
			}

			index++;
		}
	}
}

void XM_CALLCONV CGH::DX12RenderString::ChangeColor(DirectX::FXMVECTOR color)
{
	for(auto & currInfo : charInfos)
	{
		DirectX::XMStoreFloat4(&currInfo.color, color);
	}
}

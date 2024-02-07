#pragma once
#include <vector>
#include <unordered_map>
#include <DirectXMath.h>

struct CGHFontGlyphInfo
{
	uint16_t advance_width = 0;
	int16_t left_side_bearing = 0;
	int16_t bounding_box[4] = {};
	DirectX::XMFLOAT2 glyph_center = { 0,0 };
};

class CGHFontLoader
{
public:
	CGHFontLoader() = default;
	~CGHFontLoader() = default;

	void CreateFontData(const char* filePath, unsigned int curveDetail, 
		void* cdtOut, std::unordered_map<unsigned int, CGHFontGlyphInfo>* glyphInfoOut);

private:
	static void Font_parsed(void* args, void* _font_data, int error);

private:
};

#pragma once
#include <vector>
#include <unordered_map>
#include <DirectXMath.h>

struct CGHFontTriangles
{
	uint16_t advance_width;
	int16_t left_side_bearing;
	int16_t bounding_box[4];
	DirectX::XMFLOAT2 glyph_center;
	unsigned int numVertex = 0;
	unsigned int vertexOffset = 0;
	unsigned int numIndex = 0;
	unsigned int indexOffset = 0;
};

struct CGHFontData
{
	std::vector<uint16_t> languageIDs;
	std::unordered_map<unsigned int, CGHFontTriangles> fontTriangleDatas;
	std::vector<DirectX::XMFLOAT2> vertices;
	std::vector<unsigned int> indices;
};

class CGHFontLoader
{
public:
	CGHFontLoader() = default;
	~CGHFontLoader() = default;

	void CreateFontData(const char* filePath, unsigned int curveDetail, CGHFontData* out);

private:
	static void Font_parsed(void* args, void* _font_data, int error);

private:
};

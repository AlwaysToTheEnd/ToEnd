#include "CGHFontLoader.h"
#include <assert.h>

#define TTF_FONT_PARSER_IMPLEMENTATION
#include "ttfParser.h"
#include "CDT/include/CDT.h"

//https://github.com/artem-ogre/CDT

void CGHFontLoader::CreateFontData(const char* filePath, unsigned int curveDetail,
	void* cdtsOut, std::unordered_map<unsigned int, CGHFontGlyphInfo>* glyphInfoOut)
{
	uint8_t condition_variable = 0;
	TTFFontParser::FontData font_data;

	int8_t error = TTFFontParser::parse_file(filePath, &font_data, &Font_parsed, &condition_variable);
	assert(condition_variable == 1);

	 auto cdts = static_cast<std::unordered_map<unsigned int, CDT::Triangulation<float>>*>(cdtsOut);

	std::vector<CDT::V2d<float>> vertices;
	CDT::EdgeVec edges;
	for (auto& GlyphIter : font_data.glyphs)
	{
		const TTFFontParser::Glyph& currGlyph = GlyphIter.second;
		if (currGlyph.character >= '!' && currGlyph.character < 128)
		{
			assert(currGlyph.num_contours > 0);

			(*glyphInfoOut)[currGlyph.character].advance_width = currGlyph.advance_width;
			(*glyphInfoOut)[currGlyph.character].left_side_bearing = currGlyph.left_side_bearing;
			(*glyphInfoOut)[currGlyph.character].glyph_center.x = currGlyph.glyph_center.x;
			(*glyphInfoOut)[currGlyph.character].glyph_center.y = currGlyph.glyph_center.y;
			std::memcpy((*glyphInfoOut)[GlyphIter.first].bounding_box, currGlyph.bounding_box, sizeof(currGlyph.bounding_box));

			cdts->insert({ currGlyph.character, CDT::Triangulation<float>(CDT::VertexInsertionOrder::Auto, CDT::IntersectingConstraintEdges::TryResolve, 0) });
			CDT::Triangulation<float>& currCdt = (*cdts)[currGlyph.character];
			size_t indexOffset = 0;
			vertices.clear();
			edges.clear();

			for (auto& currPath : currGlyph.path_list)
			{
				unsigned int currIndex = 0;
				unsigned int numVertex = currPath.geometry.size();
				for (auto& iter : currPath.geometry)
				{
					vertices.push_back({ iter.p0.x, iter.p0.y });

					if (iter.is_curve)
					{
						CDT::V2d<float> startPoint = { iter.p0.x, iter.p0.y };
						CDT::V2d<float> endPoint = { iter.p1.x, iter.p1.y };
						CDT::V2d<float> centerPoint = { iter.c.x, iter.c.y };

						CDT::V2d<float> vec1 = { startPoint.x - centerPoint.x, startPoint.y - centerPoint.y };
						CDT::V2d<float> vec2 = { endPoint.x - centerPoint.x, endPoint.y - centerPoint.y };

						assert(curveDetail > 1);

						for (unsigned int i = 1; i < curveDetail; i++)
						{
							numVertex++;

							float t = static_cast<float>(i) / curveDetail;
							float dt = 1.0 - t;

							CDT::V2d<float> currVec1 = { vec1.x * dt, vec1.y * dt };
							CDT::V2d<float> currVec2 = { vec2.x * t, vec2.y * t };

							CDT::V2d<float> curvePoint = { (currVec2.x - currVec1.x) * t, (currVec2.y - currVec1.y) * t };
							curvePoint.x += centerPoint.x;
							curvePoint.y += centerPoint.y;

							vertices.push_back(curvePoint);
							edges.emplace_back(currIndex + indexOffset, currIndex + 1 + indexOffset);

							currIndex++;
						}
					}

					int nextIndex = (currIndex + 1) % numVertex;

					edges.emplace_back(currIndex + indexOffset, nextIndex + indexOffset);

					currIndex++;
				}

				indexOffset += numVertex;
			}

			CDT::RemoveDuplicatesAndRemapEdges(vertices, edges);
			currCdt.insertVertices(vertices);
			currCdt.insertEdges(edges);
			currCdt.eraseOuterTrianglesAndHoles();
		}
	}
}

void CGHFontLoader::Font_parsed(void* args, void* _font_data, int error)
{
	if (error)
	{
		*(uint8_t*)args = error;
	}
	else
	{
		*(uint8_t*)args = 1;
	}
}

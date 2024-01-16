#include "CGHFontLoader.h"
#include <assert.h>

#define TTF_FONT_PARSER_IMPLEMENTATION
#include "ttfParser.h"
#include "CDT/include/CDT.h"

//https://github.com/artem-ogre/CDT

void CGHFontLoader::CreateFontData(const char* filePath, unsigned int curveDetail, CGHFontData* out)
{
	uint8_t condition_variable = 0;
	TTFFontParser::FontData font_data;

	int8_t error = TTFFontParser::parse_file(filePath, &font_data, &Font_parsed, &condition_variable);
	assert(condition_variable == 1);

	for (auto& iter : font_data.font_names)
	{
		if (iter.languageID)
		{
			out->languageIDs.push_back(iter.languageID);
		}
	}

	std::unordered_map<unsigned int, CDT::Triangulation<float>> cdts;
	std::vector<CDT::V2d<float>> vertices;
	CDT::EdgeVec edges;
	for (auto& GlyphIter : font_data.glyphs)
	{
		const TTFFontParser::Glyph& currGlyph = GlyphIter.second;
		if (currGlyph.character >= '!' && currGlyph.character < 128)
		{
			assert(currGlyph.num_contours > 0);
			cdts.insert({ currGlyph.character, CDT::Triangulation<float>(CDT::VertexInsertionOrder::Auto, CDT::IntersectingConstraintEdges::TryResolve, 0) });
			CDT::Triangulation<float>& currCdt = cdts[currGlyph.character];
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

	unsigned int allNumVertex = 0;
	unsigned int allNumIndex = 0;
	for (auto& iter : cdts)
	{
		auto& currData = iter.second;
		auto& currOutData = out->fontTriangleDatas[iter.first];
		auto& currFontAttribute = font_data.glyphs[iter.first];

		currOutData.advance_width = currFontAttribute.advance_width;
		currOutData.left_side_bearing = currFontAttribute.left_side_bearing;
		std::memcpy(currOutData.bounding_box, currFontAttribute.bounding_box, sizeof(currOutData.bounding_box));
		currOutData.glyph_center.x = currFontAttribute.glyph_center.x;
		currOutData.glyph_center.y = currFontAttribute.glyph_center.y;
		currOutData.numVertex = currData.vertices.size();
		currOutData.numIndex = currData.triangles.size()*3;
		currOutData.vertexOffset = allNumVertex;
		currOutData.indexOffset = allNumIndex;

		allNumVertex += currOutData.numVertex;
		allNumIndex += currOutData.numIndex;
	}

	out->vertices.clear();
	out->indices.clear();
	out->vertices.reserve(allNumVertex);
	out->indices.reserve(allNumIndex);
	for (auto& iter : cdts)
	{
		auto& currData = iter.second;
		auto& currOutData = out->fontTriangleDatas[iter.first];
		auto& currFontAttribute = font_data.glyphs[iter.first];

		for (size_t i = 0; i < currData.vertices.size(); i++)
		{
			out->vertices.push_back({ currData.vertices[i].x , currData.vertices[i].y });
		}

		for (size_t i = 0; i < currData.triangles.size(); i++)
		{
			out->indices.push_back(currData.triangles[i].vertices[0] + currOutData.vertexOffset);
			out->indices.push_back(currData.triangles[i].vertices[1] + currOutData.vertexOffset);
			out->indices.push_back(currData.triangles[i].vertices[2] + currOutData.vertexOffset);
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

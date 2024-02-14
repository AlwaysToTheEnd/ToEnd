#include "CDT/include/CDT.h"
#include "DX12FontManager.h"
#include "CGHFontLoader.h"
#include "GraphicDeivceDX12.h"
#include "DX12DefaultBufferCreator.h"

using Microsoft::WRL::ComPtr;

DX12FontManager DX12FontManager::instance;

DX12FontManager::DX12FontManager()
{
	m_fontLoader = new CGHFontLoader;
}

DX12FontManager::~DX12FontManager()
{
	if (m_fontLoader)
	{
		delete m_fontLoader;
	}
}

const DX12Font* DX12FontManager::GetFont(const char* filePath)
{
	const DX12Font* result = nullptr;
	auto iter = m_fonts.find(filePath);

	if (iter == m_fonts.end())
	{
		result = CreateDX12Font(filePath);
	}
	else
	{
		result = &iter->second;
	}

	return result;
}

DX12Font* DX12FontManager::CreateDX12Font(const char* filePath)
{
	auto cmdQueue = GraphicDeviceDX12::GetGraphic()->GetCommandQueue();
	auto& currFont = m_fonts[filePath];

	std::unordered_map<unsigned int, CDT::Triangulation<float>> cdts;
	std::unordered_map<unsigned int, CGHFontGlyphInfo> fontInfos;

	m_fontLoader->CreateFontData(filePath, 10, &cdts, &fontInfos);

	size_t numVercies = 0;
	size_t numindics = 0;
	{
		unsigned int currIndex = 1;
		currFont.cpuFontMeshInfos.resize(fontInfos.size());

		for (auto& iter : fontInfos)
		{
			CDT::Triangulation<float>& currCdt = cdts.find(iter.first)->second;
			auto& currInfo = currFont.cpuFontMeshInfos[currIndex-1];

			currInfo.advance_width = iter.second.advance_width;
			currInfo.left_side_bearing = iter.second.left_side_bearing;
			currInfo.glyph_center = iter.second.glyph_center;
			currInfo.bounding_box[0] = iter.second.bounding_box[0];
			currInfo.bounding_box[1] = iter.second.bounding_box[1];
			currInfo.bounding_box[2] = iter.second.bounding_box[2];
			currInfo.bounding_box[3] = iter.second.bounding_box[3];

			currInfo.numVertex = currCdt.vertices.size();
			currInfo.vertexOffset = numVercies;
			currInfo.numIndex = currCdt.triangles.size() * 3;
			currInfo.indexOffset = numindics;

			numVercies += currInfo.numVertex;
			numindics += currInfo.numIndex;
			currFont.infoIndex[iter.first] = currIndex++;
		}
	}

	std::vector<unsigned int> indicesTemp;
	std::vector<CDT::V2d<float>> verticesTemp;

	indicesTemp.reserve(numindics);
	verticesTemp.reserve(numVercies);

	for (auto& currCDT : cdts)
	{
		for (auto& currTriangle : currCDT.second.triangles)
		{
			indicesTemp.push_back(currTriangle.vertices[0]);
			indicesTemp.push_back(currTriangle.vertices[1]);
			indicesTemp.push_back(currTriangle.vertices[2]);
		}

		verticesTemp.insert(verticesTemp.end(), currCDT.second.vertices.begin(), currCDT.second.vertices.end());
	}

	std::vector<D3D12_RESOURCE_DESC> resourceDescs(3);
	resourceDescs[0].Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resourceDescs[0].DepthOrArraySize = 1;
	resourceDescs[0].Format = DXGI_FORMAT_UNKNOWN;
	resourceDescs[0].Flags = D3D12_RESOURCE_FLAG_NONE;
	resourceDescs[0].Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	resourceDescs[0].Height = 1;
	resourceDescs[0].MipLevels = 1;
	resourceDescs[0].SampleDesc.Count = 1;
	resourceDescs[0].SampleDesc.Quality = 0;
	resourceDescs[0].Alignment = 0;
	resourceDescs[0].Width = sizeof(CDT::V2d<float>) * verticesTemp.size();

	resourceDescs[1] = resourceDescs[0];
	resourceDescs[1].Width = sizeof(unsigned int) * indicesTemp.size();

	resourceDescs[2] = resourceDescs[0];
	resourceDescs[2].Width = sizeof(DX12FontMeshInfo) * currFont.cpuFontMeshInfos.size();

	std::vector<const void*> datas = { verticesTemp.data(), indicesTemp.data(), currFont.cpuFontMeshInfos.data() };
	std::vector<ID3D12Resource**> resources = { currFont.vertices.GetAddressOf(), currFont.indices.GetAddressOf(), currFont.fontMeshInfos.GetAddressOf() };
	std::vector<D3D12_RESOURCE_STATES> endStates = { D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE,
		D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE };

	DX12DefaultBufferCreator::instance.CreateDefaultBuffers(datas, resourceDescs, endStates, resources);

	return &currFont;
}

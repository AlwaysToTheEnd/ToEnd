#include "Dx12FontManager.h"
#include "BinaryReader.h"
#include "DX12GarbageFrameResourceMG.h"
#include "GraphicDeivceDX12.h"
#include "DX12PipelineMG.h"
#include <ppl.h>

using namespace DirectX;

DX12FontManger DX12FontManger::s_instance;

void DX12FontManger::Init()
{
}

const CGH::DX12Font* DX12FontManger::LoadFont(const wchar_t* filePath)
{
	auto iter = s_instance.m_fonts.find(filePath);
	CGH::DX12Font* result = nullptr;

	if (iter == s_instance.m_fonts.end())
	{
		result = s_instance.CreateFontData(filePath);
	
	}
	else
	{
		result = &iter->second;
	}

	if (m_currfont != result)
	{

		m_currfont = result;
	}

	return result;
}

DX12FontManger::~DX12FontManger()
{
	
}

CGH::DX12Font* DX12FontManger::CreateFontData(const wchar_t* filePath)
{
	auto& currFont = m_fonts[filePath];
	BinaryReader reader(filePath);

	// Validate the header.
	for (char const* token = "DXTKfont"; *token; token++)
	{
		if (reader.Read<uint8_t>() != *token)
		{
			DirectX::DebugTrace("ERROR: SpriteFont provided with an invalid .spritefont file\n");
			throw std::runtime_error("Not a MakeSpriteFont output binary");
		}
	}

	// Read the glyph data.
	auto glyphCount = reader.Read<uint32_t>();
	auto glyphData = reader.ReadArray<CGH::FontGlyph>(glyphCount);

	float yOffsetCutValue = 1000.0f;
	currFont.glyphs.assign(glyphData, glyphData + glyphCount);
	{
		int prevCharCode = -5;
		int currIndex = 0;
		for (auto& iter : currFont.glyphs)
		{
			int interval = iter.character - prevCharCode;
			assert(interval > 0);

			if (iter.yOffset < yOffsetCutValue)
			{
				yOffsetCutValue = iter.yOffset;
			}

			if (interval != 1)
			{
				CGH::DX12Font::CharSubCategory category;
				category.indexOffset = currIndex;
				category.charCodeStart = iter.character;

				if (currFont.category.size())
				{
					auto& prevCategory = currFont.category.back();
					prevCategory.charNum = category.indexOffset - prevCategory.indexOffset;
				}
				else
				{
					currIndex = 0;
				}

				currFont.category.emplace_back(category);
			}

			prevCharCode = iter.character;
			currIndex++;
		}
	}

	if (yOffsetCutValue > 0.0f)
	{
		for (auto& iter : currFont.glyphs)
		{
			iter.yOffset -= yOffsetCutValue;
		}
	}

	// Read font properties.
	currFont.lineSpacing = reader.Read<float>();
	currFont.defaultChar = static_cast<wchar_t>(reader.Read<uint32_t>());

	if (currFont.defaultChar == 0)
	{
		currFont.defaultChar = 'a';
	}

	currFont.defaultGlyphIndex = currFont.GetGlyphIndex(currFont.defaultChar);
	auto& defaultGlyph = currFont.glyphs[currFont.defaultGlyphIndex];
	currFont.fontBaseHeight = defaultGlyph.yOffset + defaultGlyph.subrect.bottom - defaultGlyph.subrect.top;

	// Read the texture data.
	auto textureWidth = reader.Read<uint32_t>();
	auto textureHeight = reader.Read<uint32_t>();
	auto textureFormat = reader.Read<DXGI_FORMAT>();
	auto textureStride = reader.Read<uint32_t>();
	auto textureRows = reader.Read<uint32_t>();

	currFont.textureWidth = textureWidth;
	currFont.textureHeight = textureHeight;

	const uint64_t dataSize = uint64_t(textureStride) * uint64_t(textureRows);
	if (dataSize > UINT32_MAX)
	{
		DirectX::DebugTrace("ERROR: SpriteFont provided with an invalid .spritefont file\n");
		throw std::overflow_error("Invalid .spritefont file");
	}

	auto textureData = reader.ReadArray<uint8_t>(static_cast<size_t>(dataSize));

	Microsoft::WRL::ComPtr<ID3D12Resource> textureUploadBuffer;
	Microsoft::WRL::ComPtr<ID3D12Resource> glyphUploadBuffer;
	auto device = GraphicDeviceDX12::GetDevice();
	{
		D3D12_HEAP_PROPERTIES upProp = {};
		D3D12_RESOURCE_DESC upDesc = {};

		upProp.Type = D3D12_HEAP_TYPE_UPLOAD;

		upDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		upDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		upDesc.DepthOrArraySize = 1;
		upDesc.MipLevels = 1;
		upDesc.Height = 1;
		upDesc.Width = dataSize;
		upDesc.Format = DXGI_FORMAT_UNKNOWN;
		upDesc.Alignment = 0;
		upDesc.SampleDesc.Count = 1;
		upDesc.SampleDesc.Quality = 0;
		ThrowIfFailed(device->CreateCommittedResource(&upProp, D3D12_HEAP_FLAG_NONE,
			&upDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(textureUploadBuffer.GetAddressOf())));

		upDesc.Width = sizeof(CGH::DX12Font::GlyphForDX12) * glyphCount;
		ThrowIfFailed(device->CreateCommittedResource(&upProp, D3D12_HEAP_FLAG_NONE,
			&upDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(glyphUploadBuffer.GetAddressOf())));

		upProp.Type = D3D12_HEAP_TYPE_DEFAULT;
		ThrowIfFailed(device->CreateCommittedResource(&upProp, D3D12_HEAP_FLAG_NONE,
			&upDesc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(currFont.glyphDatas.GetAddressOf())));
	}

	{
		D3D12_HEAP_PROPERTIES texProp = {};
		D3D12_RESOURCE_DESC texDesc = {};
		texProp.Type = D3D12_HEAP_TYPE_DEFAULT;

		texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		texDesc.DepthOrArraySize = 1;
		texDesc.MipLevels = 1;
		texDesc.Height = textureHeight;
		texDesc.Width = textureWidth;
		texDesc.Format = textureFormat;
		texDesc.Alignment = 0;
		texDesc.SampleDesc.Count = 1;
		texDesc.SampleDesc.Quality = 0;
		ThrowIfFailed(device->CreateCommittedResource(&texProp, D3D12_HEAP_FLAG_NONE,
			&texDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(currFont.texture.GetAddressOf())));
	}

	{
		auto cmd = DX12GarbageFrameResourceMG::s_instance.GetGraphicsCmd();
		auto alloc = DX12GarbageFrameResourceMG::s_instance.RentCommandAllocator();
		auto queue = GraphicDeviceDX12::GetGraphic()->GetCommandQueue();

		ThrowIfFailed(cmd->Reset(alloc.Get(), nullptr));

		D3D12_RESOURCE_BARRIER bar[2] = {};
		bar[0].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		bar[0].Transition.pResource = currFont.texture.Get();
		bar[0].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
		bar[0].Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		bar[0].Transition.Subresource = 0;
		bar[1] = bar[0];
		bar[1].Transition.pResource = currFont.glyphDatas.Get();

		D3D12_SUBRESOURCE_DATA subData = {};
		subData.pData = textureData;
		subData.RowPitch = textureStride;
		subData.SlicePitch = textureStride * textureRows;
		UpdateSubresources<1>(cmd, currFont.texture.Get(), textureUploadBuffer.Get(), 0, 0, 1, &subData);

		D3D12_RESOURCE_BARRIER glyphDataBar = {};
		glyphDataBar.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		glyphDataBar.Transition.pResource = currFont.glyphDatas.Get();
		glyphDataBar.Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
		glyphDataBar.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
		glyphDataBar.Transition.Subresource = 0;

		std::vector<CGH::DX12Font::GlyphForDX12> dx12GlyphDatas;
		dx12GlyphDatas.resize(glyphCount);

		double textureWidthReciprocal = 1.0f / textureWidth;
		double textureHeightReciprocal = 1.0f / textureHeight;

		Concurrency::parallel_for(uint32_t(0), glyphCount, [&dx12GlyphDatas, &currFont, textureWidthReciprocal, textureHeightReciprocal](uint32_t index)
			{
				auto& currGlyph = currFont.glyphs[index];
				dx12GlyphDatas[index].uvLT = { (float)(currGlyph.subrect.left * textureWidthReciprocal), (float)(currGlyph.subrect.top * textureHeightReciprocal) };
				dx12GlyphDatas[index].uvRB = { (float)(currGlyph.subrect.right * textureWidthReciprocal),(float)(currGlyph.subrect.bottom * textureHeightReciprocal) };
			});
	
		subData.pData = dx12GlyphDatas.data();
		subData.RowPitch = sizeof(CGH::DX12Font::GlyphForDX12) * glyphCount;
		subData.SlicePitch = subData.RowPitch;
		cmd->ResourceBarrier(1, &glyphDataBar);
		UpdateSubresources<1>(cmd, currFont.glyphDatas.Get(), glyphUploadBuffer.Get(), 0, 0, 1, &subData);

		cmd->ResourceBarrier(_countof(bar), bar);

		ThrowIfFailed(cmd->Close());
		ID3D12CommandList* cmds[] = { cmd };
		queue->ExecuteCommandLists(1, cmds);
		DX12GarbageFrameResourceMG::s_instance.RegistGarbeges(queue, { textureUploadBuffer, glyphUploadBuffer }, alloc);
	}

	{
		D3D12_DESCRIPTOR_HEAP_DESC desc = {};
		desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		desc.NumDescriptors = 2;
		desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

		ThrowIfFailed(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(currFont.textureHeap.GetAddressOf())));

		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Format = textureFormat;
		srvDesc.Texture2D.MipLevels = 1;
		
		auto heapCpu = currFont.textureHeap->GetCPUDescriptorHandleForHeapStart();

		device->CreateShaderResourceView(currFont.texture.Get(), &srvDesc,
			heapCpu);

		heapCpu.ptr += device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		srvDesc.Format = DXGI_FORMAT_R16_UINT;
		device->CreateShaderResourceView(GraphicDeviceDX12::GetGraphic()->GetRenderIDTexture(), &srvDesc,
			heapCpu);
	}

	return &currFont;
}



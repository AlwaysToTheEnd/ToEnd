#include "Dx12FontManager.h"
#include "BinaryReader.h"
#include "DX12GarbageFrameResourceMG.h"
#include "GraphicDeivceDX12.h"

using namespace DirectX;

DX12FontManger DX12FontManger::s_instance;

const DX12FontManger::DX12Font* DX12FontManger::LoadFont(const wchar_t* filePath)
{
	auto iter = s_instance.m_fonts.find(filePath);

	if (iter == s_instance.m_fonts.end())
	{
		s_instance.CreateFontData(filePath);
		iter = s_instance.m_fonts.find(filePath);
	}

	return &iter->second;
}

void DX12FontManger::CreateFontData(const wchar_t* filePath)
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
	auto glyphData = reader.ReadArray<Glyph>(glyphCount);

	currFont.glyphs.reserve(glyphCount);

	for (size_t index = 0; index < glyphCount; index++)
	{
		currFont.glyphs[glyphData->character] = *glyphData;
		glyphData++;
	}

	// Read font properties.
	currFont.lineSpacing = reader.Read<float>();
	currFont.defaultChar = static_cast<wchar_t>(reader.Read<uint32_t>());

	// Read the texture data.
	auto textureWidth = reader.Read<uint32_t>();
	auto textureHeight = reader.Read<uint32_t>();
	auto textureFormat = reader.Read<DXGI_FORMAT>();
	auto textureStride = reader.Read<uint32_t>();
	auto textureRows = reader.Read<uint32_t>();

	const uint64_t dataSize = uint64_t(textureStride) * uint64_t(textureRows);
	if (dataSize > UINT32_MAX)
	{
		DirectX::DebugTrace("ERROR: SpriteFont provided with an invalid .spritefont file\n");
		throw std::overflow_error("Invalid .spritefont file");
	}

	auto textureData = reader.ReadArray<uint8_t>(static_cast<size_t>(dataSize));

	Microsoft::WRL::ComPtr<ID3D12Resource> uploadBuffer;
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
			&upDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(uploadBuffer.GetAddressOf())));
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

		D3D12_RESOURCE_BARRIER bar = {};
		bar.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		bar.Transition.pResource = currFont.texture.Get();
		bar.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
		bar.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		bar.Transition.Subresource = 0;

		D3D12_SUBRESOURCE_DATA subData = {};
		subData.pData = textureData;
		subData.RowPitch = textureStride;
		subData.SlicePitch = textureStride * textureRows;

		UpdateSubresources<1>(cmd, currFont.texture.Get(), uploadBuffer.Get(), 0, 0, 1, &subData);
		cmd->ResourceBarrier(1, &bar);
		
		ThrowIfFailed(cmd->Close());
		ID3D12CommandList* cmds[] = { cmd };
		queue->ExecuteCommandLists(_countof(cmds), cmds);
		DX12GarbageFrameResourceMG::s_instance.RegistGarbege(queue, uploadBuffer, alloc);
	}
}

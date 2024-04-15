#include "Dx12FontManager.h"
#include "BinaryReader.h"
#include "DX12GarbageFrameResourceMG.h"
#include "GraphicDeivceDX12.h"
#include "DX12PipelineMG.h"

using namespace DirectX;

DX12FontManger DX12FontManger::s_instance;

void DX12FontManger::Init()
{
	auto Graphic = GraphicDeviceDX12::GetGraphic();

#pragma region PIPELINE_FONT
	D3D12_ROOT_PARAMETER temp = {};
	std::vector<D3D12_ROOT_PARAMETER> rootParams;
	std::string pipeLineName = "PIPELINE_FONT";
	{
		temp.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
		temp.Constants.Num32BitValues = 5;
		temp.Constants.RegisterSpace = 0;
		temp.Constants.ShaderRegister = 0;
		temp.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		rootParams.push_back(temp);

		temp.ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
		temp.Descriptor.RegisterSpace = 0;
		temp.Descriptor.ShaderRegister = 0;
		temp.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
		rootParams.push_back(temp);

		temp.ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
		temp.Descriptor.RegisterSpace = 0;
		temp.Descriptor.ShaderRegister = 1;
		temp.ShaderVisibility = D3D12_SHADER_VISIBILITY_GEOMETRY;
		rootParams.push_back(temp);

		temp.ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
		temp.Descriptor.RegisterSpace = 0;
		temp.Descriptor.ShaderRegister = 2;
		temp.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		rootParams.push_back(temp);

		temp.ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
		temp.Descriptor.RegisterSpace = 0;
		temp.Descriptor.ShaderRegister = 3;
		temp.ShaderVisibility = D3D12_SHADER_VISIBILITY_GEOMETRY;
		rootParams.push_back(temp);

		D3D12_ROOT_SIGNATURE_DESC rootsigDesc = {};
		rootsigDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
		rootsigDesc.pStaticSamplers = s_StaticSamplers;
		rootsigDesc.NumStaticSamplers = _countof(s_StaticSamplers);
		rootsigDesc.NumParameters = rootParams.size();
		rootsigDesc.pParameters = rootParams.data();

		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
		psoDesc.NodeMask = 0;

		psoDesc.pRootSignature = DX12PipelineMG::instance.CreateRootSignature(pipeLineName.c_str(), &rootsigDesc);

		psoDesc.VS = DX12PipelineMG::instance.CreateShader(DX12_SHADER_VERTEX, (pipeLineName).c_str(), L"Shader/FontRender.hlsl", "VS");
		psoDesc.PS = DX12PipelineMG::instance.CreateShader(DX12_SHADER_PIXEL, (pipeLineName).c_str(), L"Shader/FontRender.hlsl", "PS");
		psoDesc.GS = DX12PipelineMG::instance.CreateShader(DX12_SHADER_GEOMETRY, (pipeLineName).c_str(), L"Shader/FontRender.hlsl", "GS");

		//IA Set
		psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
		psoDesc.InputLayout.NumElements = 0;
		psoDesc.InputLayout.pInputElementDescs = nullptr;

		psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
		psoDesc.SampleMask = UINT_MAX;

		psoDesc.BlendState.RenderTarget[0].BlendEnable = false;

		psoDesc.BlendState.RenderTarget[1].BlendEnable = true;
		psoDesc.BlendState.RenderTarget[1].BlendOp = D3D12_BLEND_OP_ADD;
		psoDesc.BlendState.RenderTarget[1].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
		psoDesc.BlendState.RenderTarget[1].SrcBlend = D3D12_BLEND_SRC_ALPHA;
		psoDesc.BlendState.RenderTarget[1].BlendOpAlpha = D3D12_BLEND_OP_MAX;
		psoDesc.BlendState.RenderTarget[1].DestBlendAlpha = D3D12_BLEND_ZERO;
		psoDesc.BlendState.RenderTarget[1].DestBlendAlpha = D3D12_BLEND_ONE;

		//OM Set
		psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
		psoDesc.NumRenderTargets = 2;
		psoDesc.RTVFormats[0] = DXGI_FORMAT_R16_UINT;
		psoDesc.RTVFormats[1] = DXGI_FORMAT_R8G8B8A8_UNORM;

		psoDesc.SampleDesc.Count = 1;
		psoDesc.SampleDesc.Quality = 0;

		DX12PipelineMG::instance.CreateGraphicPipeline(pipeLineName.c_str(), &psoDesc);

		PipeLineWorkSet fontWorkSet;
		fontWorkSet.pso = DX12PipelineMG::instance.GetGraphicPipeline(pipeLineName.c_str());
		fontWorkSet.rootSig = DX12PipelineMG::instance.GetRootSignature(pipeLineName.c_str());

		fontWorkSet.baseGraphicCmdFunc = [this, Graphic](ID3D12GraphicsCommandList* cmd)
		{
			if (m_currfont && m_numRenderChar)
			{
				auto currFrame = Graphic->GetCurrFrameIndex();
				auto charInfoGPU = m_charInfos->GetGPUVirtualAddress();
				auto stringInfoGPU = m_stringInfos->GetGPUVirtualAddress();

				charInfoGPU += m_maxNumChar * sizeof(CharInfo) * currFrame;
				stringInfoGPU += m_maxNumString * sizeof(StringInfo) * currFrame;

				cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_POINTLIST);
				cmd->SetGraphicsRoot32BitConstant(0, m_currfont->lineSpacing, 0);
				cmd->SetGraphicsRoot32BitConstant(0, 1.0f / m_currfont->textureWidth, 0);
				cmd->SetGraphicsRoot32BitConstant(0, 1.0f / m_currfont->textureHeight, 0);
				cmd->SetGraphicsRoot32BitConstant(0, 1.0f / GO.WIN.WindowsizeX, 1);
				cmd->SetGraphicsRoot32BitConstant(0, 1.0f / GO.WIN.WindowsizeY, 2);
				cmd->SetGraphicsRootShaderResourceView(1, m_currfont->texture->GetGPUVirtualAddress());
				cmd->SetGraphicsRootShaderResourceView(2, charInfoGPU);
				cmd->SetGraphicsRootShaderResourceView(3, stringInfoGPU);
				cmd->SetGraphicsRootShaderResourceView(4, m_currfont->glyphDatas->GetGPUVirtualAddress());

				cmd->IASetVertexBuffers(0, 0, nullptr);
				cmd->IASetIndexBuffer(nullptr);

				cmd->DrawInstanced(m_numRenderChar, 1, 0, 0);
			}
		};

		fontWorkSet.nodeGraphicCmdFunc = nullptr;
	}
}

const CGH::DX12Font* DX12FontManger::LoadFont(const wchar_t* filePath)
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
	auto glyphData = reader.ReadArray<CGH::FontGlyph>(glyphCount);

	currFont.glyphs.assign(glyphData, glyphData + glyphCount);
	{
		int prevCharCode = -5;
		int currIndex = 0;
		for (auto& iter : currFont.glyphs)
		{
			int interval = iter.character - prevCharCode;

			assert(interval > 0);

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

	// Read font properties.
	currFont.lineSpacing = reader.Read<float>();
	currFont.defaultChar = static_cast<wchar_t>(reader.Read<uint32_t>());

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

		upDesc.Width = sizeof(CGH::FontGlyph) * glyphCount;
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

		subData.pData = currFont.glyphs.data();
		subData.RowPitch = sizeof(CGH::FontGlyph) * glyphCount;
		subData.SlicePitch = subData.RowPitch;
		cmd->ResourceBarrier(1, &glyphDataBar);
		UpdateSubresources<1>(cmd, currFont.glyphDatas.Get(), glyphUploadBuffer.Get(), 0, 0, 1, &subData);

		cmd->ResourceBarrier(_countof(bar), bar);

		ThrowIfFailed(cmd->Close());
		ID3D12CommandList* cmds[] = { cmd };
		queue->ExecuteCommandLists(1, cmds);
		DX12GarbageFrameResourceMG::s_instance.RegistGarbeges(queue, { textureUploadBuffer, glyphUploadBuffer }, alloc);
	}
}

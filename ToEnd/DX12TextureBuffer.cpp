#include "DX12TextureBuffer.h"
#include "d3dx12.h"
#include "CGHGraphicResource.h"
#include "GraphicDeivceDX12.h"
#include "../Common/Source/DxException.h"
#include <DirectXTex.h>

std::unordered_map<std::string, DX12TextureBuffer::TEXTURE>	DX12TextureBuffer::s_textures;
std::unordered_map<std::string, DX12TextureBuffer::TEXTURE>	DX12TextureBuffer::s_evictedTextures;
Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>			DX12TextureBuffer::s_commandList = nullptr;
Microsoft::WRL::ComPtr<ID3D12CommandAllocator>				DX12TextureBuffer::s_comAlloc = nullptr;
Microsoft::WRL::ComPtr<ID3D12Fence>							DX12TextureBuffer::s_fence = nullptr;
UINT64														DX12TextureBuffer::s_fenceCount = 0;
UINT														DX12TextureBuffer::s_srvuavDescriptorSize = 0;

using namespace DirectX;
using namespace std;

DX12TextureBuffer::DX12TextureBuffer()
	: m_isClosed(true)
	, m_textureInfoMapped(nullptr)
{
}

DX12TextureBuffer::~DX12TextureBuffer()
{
	if (m_textureInfos != nullptr)
	{
		m_textureInfos->Unmap(0, nullptr);
	}

	for (const auto& iter : m_textures)
	{
		EvictTexture(iter->filePath);
	}
}

void DX12TextureBuffer::Init(UINT bufferSize)
{
	auto device = GraphicDeviceDX12::GetDevice();
	assert(device && bufferSize);

	if (s_commandList == nullptr)
	{
		s_srvuavDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		ThrowIfFailed(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
			IID_PPV_ARGS(s_comAlloc.GetAddressOf())));

		ThrowIfFailed(device->CreateCommandList(1, D3D12_COMMAND_LIST_TYPE_DIRECT, s_comAlloc.Get(),
			nullptr, IID_PPV_ARGS(s_commandList.GetAddressOf())));

		ThrowIfFailed(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(s_fence.GetAddressOf())));

		ThrowIfFailed(s_commandList->Close());

		ThrowIfFailed(device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(sizeof(TextureInfoData) * bufferSize),
			D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
			IID_PPV_ARGS(m_textureInfos.GetAddressOf())));

		ThrowIfFailed(m_textureInfos->Map(0, nullptr, reinterpret_cast<void**>(&m_textureInfoMapped)));
	}
}

void DX12TextureBuffer::Open()
{
	m_isClosed = false;

	ThrowIfFailed(s_commandList->Reset(s_comAlloc.Get(), nullptr));
}

void DX12TextureBuffer::AddTexture(const TextureInfo* texInfo)
{
	assert(!m_isClosed && "can not call to create texture on a closed TextureBuffer");

	const std::string& filePath = texInfo->texFilePath;
	auto device = GraphicDeviceDX12::GetDevice();
	TEXTURE* texture = ResidentTexture(filePath);

	if (texture == nullptr)
	{
		wstring wFilePath(filePath.begin(), filePath.end());
		string extension = filePath.substr(filePath.find_last_of('.') + 1);
		TexMetadata metaData;
		ScratchImage scratch;

		if (extension == "dds")
		{
			ThrowIfFailed(LoadFromDDSFile(wFilePath.c_str(), DDS_FLAGS_NONE, &metaData, scratch));
		}
		else if (extension == "tga")
		{
			ThrowIfFailed(LoadFromTGAFile(wFilePath.c_str(), &metaData, scratch));
		}
		else
		{
			ThrowIfFailed(LoadFromWICFile(wFilePath.c_str(), WIC_FLAGS_NONE, &metaData, scratch));
		}

		texture = &s_textures[filePath];
		texture->filePath = filePath;
		texture->refCount = 1;

		DataToDevice(device, metaData, scratch, *texture);
	}
	
	TextureInfoData infodata;
	infodata.type = texInfo->type;
	infodata.uvIndex = texInfo->uvIndex;
	infodata.textureOp = texInfo->textureOp;
	infodata.blend = texInfo->blend;
	infodata.mapping = texInfo->mapping;
	std::memcpy(infodata.mapMode, texInfo->mapMode, sizeof(infodata.mapMode));

	std::memcpy(m_textureInfoMapped + m_textures.size(), &infodata, sizeof(TextureInfoData));
	m_textures.push_back(texture);
}

void DX12TextureBuffer::Close()
{
	assert(!m_isClosed && "TextureHeap already closed");

	ThrowIfFailed(s_commandList->Close());

	ID3D12CommandList* commandLists[] = { s_commandList.Get() };
	auto queue = GraphicDeviceDX12::GetGraphic()->GetCommandQueue();
	queue->ExecuteCommandLists(1, commandLists);
	
	s_fenceCount++;
	ThrowIfFailed(queue->Signal(s_fence.Get(), s_fenceCount));

	if (s_fence->GetCompletedValue() < s_fenceCount)
	{
		HANDLE eventHandle = CreateEventEx(nullptr, nullptr, false, EVENT_ALL_ACCESS);
		ThrowIfFailed(s_fence->SetEventOnCompletion(s_fenceCount, eventHandle));

		if (eventHandle)
		{
			WaitForSingleObject(eventHandle, INFINITE);
			CloseHandle(eventHandle);
		}
		else
		{
			assert(false);
		}
	}

	m_uploadBuffers.clear();
}

void DX12TextureBuffer::CreateSRVs(D3D12_CPU_DESCRIPTOR_HANDLE srvHeapHandle)
{
	auto device = GraphicDeviceDX12::GetDevice();

	D3D12_RESOURCE_DESC texDesc = {};
	D3D12_SHADER_RESOURCE_VIEW_DESC desc = {};

	for (auto& iter : m_textures)
	{
		texDesc = iter->tex->GetDesc();
		desc.Format = texDesc.Format;
		desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

		switch (texDesc.Dimension)
		{
		case D3D12_RESOURCE_DIMENSION_TEXTURE1D:
		{
			desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE1D;
			desc.Texture1D.MipLevels = texDesc.MipLevels;
			desc.Texture1D.MostDetailedMip = 0;
			desc.Texture1D.ResourceMinLODClamp = 0;
		}
			break;
		case D3D12_RESOURCE_DIMENSION_TEXTURE2D:
		{
			desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			desc.Texture2D.MipLevels = texDesc.MipLevels;
			desc.Texture2D.MostDetailedMip = 0;
			desc.Texture2D.ResourceMinLODClamp = 0;
			desc.Texture2D.PlaneSlice = 0;
		}
			break;
		case D3D12_RESOURCE_DIMENSION_TEXTURE3D:
		{
			desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
			desc.Texture3D.MipLevels = texDesc.MipLevels;
			desc.Texture3D.MostDetailedMip = 0;
			desc.Texture3D.ResourceMinLODClamp = 0;
		}
			break;
		default:
			assert(false);
			break;
		}

		device->CreateShaderResourceView(iter->tex.Get(), &desc, srvHeapHandle);
		srvHeapHandle.ptr += s_srvuavDescriptorSize;
	}
}

void DX12TextureBuffer::DataToDevice(ID3D12Device* device,
	const DirectX::TexMetadata& metadata,
	const DirectX::ScratchImage& scratch,
	TEXTURE& target)
{
	const uint8_t* pixelMemory = scratch.GetPixels();
	const Image* image = scratch.GetImages();
	const size_t imageNum = scratch.GetImageCount();

	CD3DX12_HEAP_PROPERTIES textureHP(D3D12_HEAP_TYPE_DEFAULT);
	CD3DX12_HEAP_PROPERTIES uploadHP(D3D12_HEAP_TYPE_UPLOAD);
	D3D12_RESOURCE_DESC textureDesc = {};
	D3D12_RESOURCE_DESC uploaderDesc = {};
	CD3DX12_RANGE range(0, scratch.GetPixelsSize());

	textureDesc.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
	textureDesc.DepthOrArraySize = static_cast<UINT16>(metadata.arraySize);
	textureDesc.Dimension = static_cast<D3D12_RESOURCE_DIMENSION>(metadata.dimension);
	textureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
	textureDesc.Format = metadata.format;
	textureDesc.Height = static_cast<UINT>(metadata.height);
	textureDesc.Width = static_cast<UINT>(metadata.width);
	textureDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	textureDesc.MipLevels = static_cast<UINT16>(metadata.mipLevels);
	textureDesc.SampleDesc.Count = 1;
	textureDesc.SampleDesc.Quality = 0;

	ThrowIfFailed(device->CreateCommittedResource(
		&textureHP,
		D3D12_HEAP_FLAG_NONE,
		&textureDesc,
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(target.tex.GetAddressOf())));

	uploaderDesc = textureDesc;

	uploaderDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	uploaderDesc.Height = 1;
	uploaderDesc.Width = range.End;
	uploaderDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	uploaderDesc.Format = DXGI_FORMAT_UNKNOWN;

	m_uploadBuffers.emplace_back();
	ThrowIfFailed(device->CreateCommittedResource(
		&uploadHP,
		D3D12_HEAP_FLAGS::D3D12_HEAP_FLAG_NONE,
		&uploaderDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(m_uploadBuffers.back().GetAddressOf())));

	D3D12_SUBRESOURCE_DATA subData;
	subData.pData = pixelMemory;
	subData.RowPitch = image->rowPitch;
	subData.SlicePitch = image->slicePitch;
	UpdateSubresources<1>(s_commandList.Get(), target.tex.Get(),
		m_uploadBuffers.back().Get(), 0, 0, 1, &subData);
}

void DX12TextureBuffer::EvictTexture(const std::string& filePath)
{
	auto iter = s_textures.find(filePath);

	assert(iter != s_textures.end());

	iter->second.refCount--;

	if (iter->second.refCount < 1)
	{
		assert(s_evictedTextures.find(filePath) == s_evictedTextures.end());

		s_evictedTextures[filePath] = iter->second;
		ID3D12Pageable* const resource = s_evictedTextures[filePath].tex.Get();

		s_textures.erase(iter);
		
		GraphicDeviceDX12::GetDevice()->Evict(1, &resource);
	}
}

DX12TextureBuffer::TEXTURE* DX12TextureBuffer::ResidentTexture(const std::string& filePath)
{
	TEXTURE* result = nullptr;

	auto iter = s_textures.find(filePath);
	if (iter == s_textures.end())
	{
		iter = s_evictedTextures.find(filePath);

		if (iter != s_evictedTextures.end()) 
		{
			s_textures[filePath] = iter->second;
			result = &s_textures[filePath];
			result->refCount = 1;

			s_evictedTextures.erase(iter);

			ID3D12Pageable* const resource = result->tex.Get();
			GraphicDeviceDX12::GetDevice()->MakeResident(1, &resource);
		}
	}
	else
	{
		result = &iter->second;
		iter->second.refCount++;
	}

	return result;
}
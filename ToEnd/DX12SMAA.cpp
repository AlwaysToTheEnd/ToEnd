#include "DX12SMAA.h"
#include <DirectXTex.h>
#include "d3dx12.h"
#include "../Common/Source/DxException.h"
#include "DX12GarbageFrameResourceMG.h"

std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> DX12SMAA::Resize(ID3D12Device* device,
	ID3D12GraphicsCommandList* cmd, unsigned int winX, unsigned int winY, ID3D12Resource* diffuseTexture)
{
	std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> upBuffers;
	m_textures[TEXTYPE_EDGES].Reset();
	m_textures[TEXTYPE_BLEND].Reset();

	if (m_srvHeaps == nullptr)
	{
		D3D12_DESCRIPTOR_HEAP_DESC desc = {};
		desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		desc.NodeMask = 0;
		desc.NumDescriptors = TEXTYPE_NUM + 2;
		desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		
		ThrowIfFailed(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(m_srvHeaps.GetAddressOf())));

		desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		desc.NodeMask = 0;
		desc.NumDescriptors = 2;
		desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		ThrowIfFailed(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(m_rtvHeaps.GetAddressOf())));
	}

	D3D12_RESOURCE_DESC desc = {};
	D3D12_HEAP_PROPERTIES prop = {};

	prop.Type = D3D12_HEAP_TYPE_DEFAULT;
	prop.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	prop.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	prop.CreationNodeMask = 0;
	prop.VisibleNodeMask = 0;

	desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	desc.Alignment = 0;
	desc.Width = winX;
	desc.Height = winY;
	desc.DepthOrArraySize = 1;
	desc.MipLevels = 0;
	desc.SampleDesc.Count = 1;
	desc.SampleDesc.Quality = 0;
	desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
	desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;

	D3D12_CLEAR_VALUE clearVal = {};
	clearVal.Format = DXGI_FORMAT_R8G8B8A8_UNORM;

	ThrowIfFailed(device->CreateCommittedResource(&prop, D3D12_HEAP_FLAG_NONE, &desc,
		D3D12_RESOURCE_STATE_RENDER_TARGET, &clearVal, IID_PPV_ARGS(m_textures[TEXTYPE_EDGES] .GetAddressOf())));

	ThrowIfFailed(device->CreateCommittedResource(&prop, D3D12_HEAP_FLAG_NONE, &desc,
		D3D12_RESOURCE_STATE_RENDER_TARGET, &clearVal, IID_PPV_ARGS(m_textures[TEXTYPE_BLEND].GetAddressOf())));

	if (m_textures[TEXTYPE_SEARCHTH] == nullptr)
	{
		upBuffers.resize(2);
		D3D12_RESOURCE_DESC upDesc = {};
		D3D12_HEAP_PROPERTIES upProp = {};
		upProp.Type = D3D12_HEAP_TYPE_UPLOAD;

		upDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		upDesc.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
		upDesc.Format = DXGI_FORMAT_UNKNOWN;
		upDesc.Height = 1;
		upDesc.DepthOrArraySize = 1;
		upDesc.MipLevels = 1;
		upDesc.SampleDesc.Count = 1;
		upDesc.SampleDesc.Quality = 0;
		upDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

		{
			DirectX::TexMetadata metaData;
			DirectX::ScratchImage scratch;
			ThrowIfFailed(DirectX::LoadFromDDSFile(L"Textures/SMAA/AreaTex.dds", DirectX::DDS_FLAGS_NONE, &metaData, scratch));

			const uint8_t* pixelMemory = scratch.GetPixels();
			const DirectX::Image* image = scratch.GetImages();
			const size_t imageNum = scratch.GetImageCount();

			desc.Format = image->format;
			upDesc.Width = scratch.GetPixelsSize();

			ThrowIfFailed(device->CreateCommittedResource(&upProp, D3D12_HEAP_FLAG_NONE, &upDesc,
				D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(upBuffers[0].GetAddressOf())));

			ThrowIfFailed(device->CreateCommittedResource(&prop, D3D12_HEAP_FLAG_NONE, &desc,
				D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(m_textures[TEXTYPE_AREA].GetAddressOf())));

			D3D12_SUBRESOURCE_DATA subData;
			subData.pData = pixelMemory;
			subData.RowPitch = image->rowPitch;
			subData.SlicePitch = image->slicePitch;

			UpdateSubresources<1>(cmd, m_textures[TEXTYPE_AREA].Get(),
				upBuffers[0].Get(), 0, 0, 1, &subData);
		}
		
		{
			DirectX::TexMetadata metaData;
			DirectX::ScratchImage scratch;
			ThrowIfFailed(DirectX::LoadFromDDSFile(L"Textures/SMAA/SearchTex.dds", DirectX::DDS_FLAGS_NONE, &metaData, scratch));

			const uint8_t* pixelMemory = scratch.GetPixels();
			const DirectX::Image* image = scratch.GetImages();
			const size_t imageNum = scratch.GetImageCount();

			desc.Format = image->format;
			upDesc.Width = scratch.GetPixelsSize();

			ThrowIfFailed(device->CreateCommittedResource(&upProp, D3D12_HEAP_FLAG_NONE, &upDesc,
				D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(upBuffers[1].GetAddressOf())));

			ThrowIfFailed(device->CreateCommittedResource(&prop, D3D12_HEAP_FLAG_NONE, &desc,
				D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(m_textures[TEXTYPE_SEARCHTH].GetAddressOf())));

			D3D12_SUBRESOURCE_DATA subData;
			subData.pData = pixelMemory;
			subData.RowPitch = image->rowPitch;
			subData.SlicePitch = image->slicePitch;

			UpdateSubresources<1>(cmd, m_textures[TEXTYPE_SEARCHTH].Get(),
				upBuffers[1].Get(), 0, 0, 1, &subData);
		}
	}

	int srvSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	int rtvSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	auto srvHeapCPU = m_srvHeaps->GetCPUDescriptorHandleForHeapStart();

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	{
		srvDesc.Texture2D.MipLevels = 1;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

		device->CreateShaderResourceView(diffuseTexture, &srvDesc, srvHeapCPU);

		srvHeapCPU.ptr += srvSize;
		srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		device->CreateShaderResourceView(diffuseTexture, &srvDesc, srvHeapCPU);
	}
	
	for (int i = 0; i < TEXTYPE_NUM; i++)
	{
		srvHeapCPU.ptr += srvSize;
		srvDesc.Format = m_textureFormats[i];
		device->CreateShaderResourceView(m_textures[i].Get(), &srvDesc, srvHeapCPU);
	}

	auto rtvHeapCPU = m_rtvHeaps->GetCPUDescriptorHandleForHeapStart();
	for (int i = TEXTYPE_EDGES; i < TEXTYPE_NUM; i++)
	{
		D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
		rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
		rtvDesc.Format = m_textureFormats[i];

		device->CreateRenderTargetView(m_textures[i].Get(), &rtvDesc, rtvHeapCPU);
		rtvHeapCPU.ptr += rtvSize;
	}

	return upBuffers;
}

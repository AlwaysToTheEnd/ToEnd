#include "DX12SMAA.h"
#include <DirectXTex.h>
#include "d3dx12.h"
#include "../Common/Source/DxException.h"
#include "DX12GarbageFrameResourceMG.h"

ID3D12DescriptorHeap* DX12SMAA::GetSRVHeap()
{
	return m_srvHeaps.Get();
}

D3D12_CPU_DESCRIPTOR_HANDLE DX12SMAA::GetEdgeRenderTarget()
{
	return m_rtvHeaps->GetCPUDescriptorHandleForHeapStart();
}

D3D12_CPU_DESCRIPTOR_HANDLE DX12SMAA::GetBlendRenderTarget()
{
	auto result = m_rtvHeaps->GetCPUDescriptorHandleForHeapStart();
	result.ptr += m_rtvSize;
	return result;
}

D3D12_CPU_DESCRIPTOR_HANDLE DX12SMAA::GetColorRenderTarget()
{
	auto result = m_rtvHeaps->GetCPUDescriptorHandleForHeapStart();
	result.ptr += m_rtvSize *2;
	return result;
}

std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> DX12SMAA::Resize(ID3D12Device* device,
	ID3D12GraphicsCommandList* cmd, unsigned int winX, unsigned int winY)
{
	std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> upBuffers;
	m_textures[TEXTYPE_EDGES].Reset();
	m_textures[TEXTYPE_BLEND].Reset();
	m_textures[TEXTYPE_COLOR].Reset();
	
	if (m_srvHeaps == nullptr)
	{
		D3D12_DESCRIPTOR_HEAP_DESC desc = {};
		desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		desc.NodeMask = 0;
		desc.NumDescriptors = TEXTYPE_COLOR + 2;
		desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		
		ThrowIfFailed(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(m_srvHeaps.GetAddressOf())));

		desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		desc.NodeMask = 0;
		desc.NumDescriptors = 3;
		desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		ThrowIfFailed(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(m_rtvHeaps.GetAddressOf())));
	}

	D3D12_RESOURCE_DESC desc = {};
	D3D12_HEAP_PROPERTIES prop = {};

	prop.Type = D3D12_HEAP_TYPE_DEFAULT;
	prop.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	prop.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	prop.CreationNodeMask = 1;
	prop.VisibleNodeMask = 1;

	desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	desc.Alignment = 0;
	desc.Width = winX;
	desc.Height = winY;
	desc.DepthOrArraySize = 1;
	desc.MipLevels = 1;
	desc.SampleDesc.Count = 1;
	desc.SampleDesc.Quality = 0;
	desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
	desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;

	D3D12_CLEAR_VALUE clearVal = {};
	clearVal.Format = DXGI_FORMAT_R8G8_UNORM;

	desc.Format = DXGI_FORMAT_R8G8_UNORM;
	ThrowIfFailed(device->CreateCommittedResource(&prop, D3D12_HEAP_FLAG_NONE, &desc,
		D3D12_RESOURCE_STATE_RENDER_TARGET, &clearVal, IID_PPV_ARGS(m_textures[TEXTYPE_EDGES].GetAddressOf())));

	desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	clearVal.Format = DXGI_FORMAT_R8G8B8A8_UNORM;

	ThrowIfFailed(device->CreateCommittedResource(&prop, D3D12_HEAP_FLAG_NONE, &desc,
		D3D12_RESOURCE_STATE_RENDER_TARGET, &clearVal, IID_PPV_ARGS(m_textures[TEXTYPE_COLOR].GetAddressOf())));

	ThrowIfFailed(device->CreateCommittedResource(&prop, D3D12_HEAP_FLAG_NONE, &desc,
		D3D12_RESOURCE_STATE_RENDER_TARGET, &clearVal, IID_PPV_ARGS(m_textures[TEXTYPE_BLEND].GetAddressOf())));

	if (m_textures[TEXTYPE_SEARCH] == nullptr)
	{
		upBuffers.resize(2);
		D3D12_RESOURCE_DESC upDesc = {};
		D3D12_HEAP_PROPERTIES upProp = {};
		upProp.Type = D3D12_HEAP_TYPE_UPLOAD;
		upProp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		upProp.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
		upProp.CreationNodeMask = 1;
		upProp.VisibleNodeMask = 1;

		upDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		upDesc.Alignment = 0;
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
			
			desc.Width = image->width;
			desc.Height = image->height;
			desc.Format = image->format;
			desc.Flags = D3D12_RESOURCE_FLAG_NONE;
			upDesc.Width = scratch.GetPixelsSize() * 1.5;

			ThrowIfFailed(device->CreateCommittedResource(&upProp, D3D12_HEAP_FLAG_NONE, &upDesc,
				D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(upBuffers[0].GetAddressOf())));

			ThrowIfFailed(device->CreateCommittedResource(&prop, D3D12_HEAP_FLAG_NONE, &desc,
				D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(m_textures[TEXTYPE_AREA].GetAddressOf())));

			D3D12_SUBRESOURCE_DATA subData;
			subData.pData = pixelMemory;
			subData.RowPitch = image->rowPitch;
			subData.SlicePitch = image->slicePitch;

			UpdateSubresources<1>(cmd, m_textures[TEXTYPE_AREA].Get(),
				upBuffers[0].Get(), 0, 0, 1, &subData);

			m_textures[TEXTYPE_AREA]->SetName(L"SMAA_AREA");
			cmd->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_textures[TEXTYPE_AREA].Get(),
				D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ));
		}
		
		{
			DirectX::TexMetadata metaData;
			DirectX::ScratchImage scratch;
			ThrowIfFailed(DirectX::LoadFromDDSFile(L"Textures/SMAA/SearchTex.dds", DirectX::DDS_FLAGS_NO_LEGACY_EXPANSION, &metaData, scratch));

			const uint8_t* pixelMemory = scratch.GetPixels();
			const DirectX::Image* image = scratch.GetImages();
			const size_t imageNum = scratch.GetImageCount();

			desc.Width = image->width;
			desc.Height = image->height;
			desc.Format = image->format;
			desc.Flags = D3D12_RESOURCE_FLAG_NONE;
			upDesc.Width = scratch.GetPixelsSize() * 1.5;

			ThrowIfFailed(device->CreateCommittedResource(&upProp, D3D12_HEAP_FLAG_NONE, &upDesc,
				D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(upBuffers[1].GetAddressOf())));

			ThrowIfFailed(device->CreateCommittedResource(&prop, D3D12_HEAP_FLAG_NONE, &desc,
				D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(m_textures[TEXTYPE_SEARCH].GetAddressOf())));

			D3D12_SUBRESOURCE_DATA subData;
			subData.pData = pixelMemory;
			subData.RowPitch = image->rowPitch;
			subData.SlicePitch = image->slicePitch;

			UpdateSubresources<1>(cmd, m_textures[TEXTYPE_SEARCH].Get(),
				upBuffers[1].Get(), 0, 0, 1, &subData);

			m_textures[TEXTYPE_SEARCH]->SetName(L"SMAA_SEARCH");
			cmd->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_textures[TEXTYPE_SEARCH].Get(),
				D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ));
		}
	}

	int srvSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	m_rtvSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	auto srvHeapCPU = m_srvHeaps->GetCPUDescriptorHandleForHeapStart();

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	{
		srvDesc.Texture2D.MipLevels = 1;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

		device->CreateShaderResourceView(m_textures[TEXTYPE_COLOR].Get(), &srvDesc, srvHeapCPU);

		srvHeapCPU.ptr += srvSize;
		srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		device->CreateShaderResourceView(m_textures[TEXTYPE_COLOR].Get(), &srvDesc, srvHeapCPU);
	}
	
	for (int i = 0; i < TEXTYPE_COLOR; i++)
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
		rtvHeapCPU.ptr += m_rtvSize;
	}

	return upBuffers;
}

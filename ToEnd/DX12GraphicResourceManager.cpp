#include "DX12GraphicResourceManager.h"
#include "GraphicDeivceDX12.h"

DX12GraphicResourceManager DX12GraphicResourceManager::s_insatance;

void DX12GraphicResourceManager::Init()
{
	m_device = GraphicDeviceDX12::GetDevice();
}

void DX12GraphicResourceManager::CreateResource(unsigned int dataStride, GraphicData* data)
{
	data->stride = (dataStride + 255) & ~255;

	auto hr = m_device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(static_cast<UINT64>(dataStride) * baseNumData),
		D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
		IID_PPV_ARGS(data->gpuData.GetAddressOf()));

	if (FAILED(hr))
	{
		ThrowIfFailed(m_device->GetDeviceRemovedReason());
	}

	ThrowIfFailed(data->gpuData->Map(0, nullptr, reinterpret_cast<void**>(&data->cpuData)));
}

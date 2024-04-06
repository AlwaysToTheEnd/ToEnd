#include "DX12GraphicResourceManager.h"
#include <numeric>
#include "GraphicDeivceDX12.h"

DX12GraphicResourceManager DX12GraphicResourceManager::s_insatance;

void DX12GraphicResourceManager::Init()
{
	m_device = GraphicDeviceDX12::GetDevice();
}

void DX12GraphicResourceManager::CreateResource(unsigned int dataStride, GraphicData* data)
{
	unsigned int numFrameResource = GraphicDeviceDX12::GetGraphic()->GetNumFrameResource();
	data->stride = (dataStride + 255) & ~255;
	data->refCount.resize(baseNumData);

	auto hr = m_device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(static_cast<UINT64>(data->stride) * baseNumData * numFrameResource),
		D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
		IID_PPV_ARGS(data->gpuData.GetAddressOf()));

	if (FAILED(hr))
	{
		ThrowIfFailed(m_device->GetDeviceRemovedReason());
	}

	data->releasedIndices.reserve(baseNumData);
	for (int i = baseNumData - 1; i >= 0; i--)
	{
		data->releasedIndices.push_back(i);
	}

	BYTE* cpuData = nullptr;
	ThrowIfFailed(data->gpuData->Map(0, nullptr, reinterpret_cast<void**>(&cpuData)));

	for (int i = 0; i < numFrameResource; i++)
	{
		data->cpuDatas.push_back(cpuData);
		cpuData += data->stride * baseNumData;
	}
}

void DX12GraphicResourceManager::SetElementData(unsigned int index, unsigned int dataSize, const void* data, GraphicData* graphicData)
{
	if (graphicData->refCount[index])
	{
		unsigned int currFrameIndex = GraphicDeviceDX12::GetGraphic()->GetCurrFrameIndex();
		BYTE* cpudata = graphicData->cpuDatas[currFrameIndex];
		cpudata += index * graphicData->stride;
		std::memcpy(cpudata, data, dataSize);
	}
}


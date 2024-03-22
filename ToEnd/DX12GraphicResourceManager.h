#pragma once
#include <d3d12.h>
#include <unordered_map>
#include <vector>
#include <string>
#include <unordered_map>
#include <wrl.h>
#include <assert.h>

class DX12GraphicResourceManager
{
	struct GraphicData
	{
		unsigned int stride = 0;
		std::vector<unsigned int> releasedIndices;
		BYTE* cpuData = nullptr;
		Microsoft::WRL::ComPtr<ID3D12Resource> gpuData;
	};
public:
	static const int baseNumData = 256;
	static DX12GraphicResourceManager s_insatance;

public:
	void Init();

	template<typename T> T* CreateData(unsigned int* indexOut);
	template<typename T> unsigned int GetIndex(T* data);
	template<typename T> void ReleaseData(T* data);
	template<typename T> unsigned int AddRef(T* data);

	template<typename T> D3D12_GPU_VIRTUAL_ADDRESS GetGpuStartAddress();
	template<typename T> D3D12_GPU_VIRTUAL_ADDRESS GetGpuAddress(T* data);
	template<typename T> D3D12_GPU_VIRTUAL_ADDRESS GetGpuAddress(size_t index);

private:
	DX12GraphicResourceManager() = default;
	~DX12GraphicResourceManager() = default;

	void CreateResource(unsigned int dataStride, GraphicData* dataOut);

private:
	std::unordered_map<size_t, GraphicData> m_datas;
	std::unordered_map<void*, int> m_refCount;
	ID3D12Device* m_device = nullptr;
};


template<typename T>
inline T* DX12GraphicResourceManager::CreateData(unsigned int* indexOut)
{
	T* result = nullptr;
	GraphicData& datas = m_datas[typeid(T).hash_code()];

	if (datas.gpuData == nullptr)
	{
		CreateResource(sizeof(T), &datas);
	}

	unsigned int index = datas.releasedIndices.back();
	datas.releasedIndices.pop_back();

	if (indexOut)
	{
		*indexOut = index;
	}

	result = (T*)(datas.cpuData + index * datas.stride);
	m_refCount[result] = 1;

	return result;
}

template<typename T>
inline unsigned int DX12GraphicResourceManager::GetIndex(T* data)
{
	GraphicData& datas = m_datas[typeid(T).hash_code()];

	unsigned int index = ((size_t)data - (size_t)datas.cpuData) / datas.stride;
	assert(index < baseNumData);

	return index;
}

template<typename T>
inline void DX12GraphicResourceManager::ReleaseData(T* data)
{
	auto iter = m_refCount.find(data);

	if (iter != m_refCount.end())
	{
		iter->second -= 1;
		
		if (iter->second < 1)
		{
			GraphicData& datas = m_datas[typeid(T).hash_code()];

			unsigned int index = ((size_t)data - (size_t)datas.cpuData) / datas.stride;
			assert(index < baseNumData);

			datas.releasedIndices.push_back(index);
			m_refCount.erase(iter);
		}
	}
}

template<typename T>
inline unsigned int DX12GraphicResourceManager::AddRef(T* data)
{
	unsigned int result = 0;

	auto iter = m_refCount.find(data);

	if (iter != m_refCount.end())
	{
		iter->second += 1;
		result = iter->second;
	}

	return result;
}

template<typename T>
inline D3D12_GPU_VIRTUAL_ADDRESS DX12GraphicResourceManager::GetGpuStartAddress()
{
	D3D12_GPU_VIRTUAL_ADDRESS result = 0;
	GraphicData& datas = m_datas[typeid(T).hash_code()];
	return datas.gpuData->GetGPUVirtualAddress();
}

template<typename T>
inline D3D12_GPU_VIRTUAL_ADDRESS DX12GraphicResourceManager::GetGpuAddress(T* data)
{
	D3D12_GPU_VIRTUAL_ADDRESS result = 0;
	GraphicData& datas = m_datas[typeid(T).hash_code()];

	unsigned int index = ((size_t)data - (size_t)datas.cpuData) / datas.stride;

	assert(index < baseNumData);

	result = datas.gpuData->GetGPUVirtualAddress() + (index * datas.stride);

	return result();
}

template<typename T>
inline D3D12_GPU_VIRTUAL_ADDRESS DX12GraphicResourceManager::GetGpuAddress(size_t index)
{
	D3D12_GPU_VIRTUAL_ADDRESS result = 0;
	GraphicData& datas = m_datas[typeid(T).hash_code()];

	result = datas.gpuData->GetGPUVirtualAddress() + (index * datas.stride);

	return result;
}
#pragma once
#include <d3d12.h>
#include <unordered_map>
#include <vector>
#include <string>
#include <wrl.h>
#include <assert.h>


class DX12GraphicResourceManager
{
	struct GraphicData
	{
		unsigned int stride = 0;
		std::unordered_map<std::string, size_t> names;
		BYTE* cpuData = nullptr;
		Microsoft::WRL::ComPtr<ID3D12Resource> gpuData;
	};
public:
	static DX12GraphicResourceManager s_insatance;

public:
	void Init();

	template<typename T> const T* GetData(const char* name, size_t* indexOut = nullptr);
	template<typename T> const T* GetData(size_t index);
	template<typename T> const size_t GetSize();
	template<typename T> const D3D12_GPU_VIRTUAL_ADDRESS GetGpuStartAddress();
	template<typename T> const D3D12_GPU_VIRTUAL_ADDRESS GetGpuAddress(size_t index);
	template<typename T> const D3D12_GPU_VIRTUAL_ADDRESS GetGpuAddress(const char* name);

	template<typename T> size_t SetData(const char* name, const T* data);

private:
	DX12GraphicResourceManager() = default;
	~DX12GraphicResourceManager() = default;

	void CreateResource(unsigned int dataStride, GraphicData* dataOut);

private:
	std::unordered_map<size_t, GraphicData> m_datas;
	ID3D12Device* m_device = nullptr;
	const int baseNumData = 128;
};

template<typename T>
inline const T* DX12GraphicResourceManager::GetData(const char* name, size_t* indexOut)
{
	const T* result = nullptr;

	GraphicData& datas = m_datas[typeid(T).hash_code()];

	auto iter = datas.names.find(name);

	if (iter != datas.names.end() && datas.cpuData != nullptr)
	{
		result = (T*)(datas.cpuData + iter->second * datas.stride);

		if (indexOut)
		{
			*indexOut = iter->second;
		}
	}

	return result;
}

template<typename T>
inline const T* DX12GraphicResourceManager::GetData(size_t index)
{
	const T* result = nullptr;

	GraphicData& datas = m_datas[typeid(T).hash_code()];

	if (datas.names.size() > index && datas.cpuData != nullptr)
	{
		result = (T*)(datas.cpuData + index * datas.stride);
	}

	return result;
}

template<typename T>
inline const size_t DX12GraphicResourceManager::GetSize()
{
	D3D12_GPU_VIRTUAL_ADDRESS result = 0;
	GraphicData& datas = m_datas[typeid(T).hash_code()];

	return datas.names.size();
}

template<typename T>
inline const D3D12_GPU_VIRTUAL_ADDRESS DX12GraphicResourceManager::GetGpuStartAddress()
{
	D3D12_GPU_VIRTUAL_ADDRESS result = 0;
	GraphicData& datas = m_datas[typeid(T).hash_code()];
	return datas.gpuData->GetGPUVirtualAddress();
}

template<typename T>
inline const D3D12_GPU_VIRTUAL_ADDRESS DX12GraphicResourceManager::GetGpuAddress(size_t index)
{
	D3D12_GPU_VIRTUAL_ADDRESS result = 0;
	GraphicData& datas = m_datas[typeid(T).hash_code()];

	if (datas.names.size() > index)
	{
		result = datas.gpuData->GetGPUVirtualAddress() + (index * datas.stride);
	}

	return result;
}

template<typename T>
inline const D3D12_GPU_VIRTUAL_ADDRESS DX12GraphicResourceManager::GetGpuAddress(const char* name)
{
	D3D12_GPU_VIRTUAL_ADDRESS result = 0;
	GraphicData& datas = m_datas[typeid(T).hash_code()];

	auto iter = datas.names.find(name);

	if (iter != datas.names.end())
	{
		result = datas.gpuData->GetGPUVirtualAddress() + (iter->second * datas.stride);
	}

	return result;
}

template<typename T>
inline size_t DX12GraphicResourceManager::SetData(const char* name, const T* data)
{
	size_t index = 0;
	GraphicData& datas = m_datas[typeid(T).hash_code()];
	size_t dataSize = sizeof(T);

	if (datas.gpuData == nullptr)
	{
		CreateResource(dataSize, &datas);
	}

	auto iter = datas.names.find(name);

	if (iter == datas.names.end())
	{
		index = datas.names.size();
		assert(index < baseNumData);
		datas.names[name] = index;
	}
	else
	{
		index = iter->second;
	}
	
	std::memcpy(datas.cpuData.data() + datas.stride * index, data, dataSize);
	return index;
}

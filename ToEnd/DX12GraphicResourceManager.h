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
		std::vector<int> refCount;
		std::vector<BYTE*> cpuDatas;
		Microsoft::WRL::ComPtr<ID3D12Resource> gpuData;
	};
public:
	static const int baseNumData = 256;
	static DX12GraphicResourceManager s_insatance;

public:
	void Init();

	template<typename T> unsigned int CreateData();
	template<typename T> void ReleaseData(unsigned int index);
	template<typename T> void AddRef(unsigned int index);
	template<typename T> void SetData(unsigned int index, const T* data);

	template<typename T> D3D12_GPU_VIRTUAL_ADDRESS GetGpuStartAddress(unsigned int currFrame);
	template<typename T> D3D12_GPU_VIRTUAL_ADDRESS GetGpuAddress(size_t index, unsigned int currFrame);

private:
	DX12GraphicResourceManager() = default;
	~DX12GraphicResourceManager() = default;

	void CreateResource(unsigned int dataStride, GraphicData* dataOut);
	void SetElementData(unsigned int index, unsigned int dataSize,const void* data, GraphicData* graphicData);

private:
	std::unordered_map<size_t, GraphicData> m_datas;
	ID3D12Device* m_device = nullptr;
};


template<typename T>
inline unsigned int DX12GraphicResourceManager::CreateData()
{
	unsigned int result = 0;
	GraphicData& datas = m_datas[typeid(T).hash_code()];

	if (datas.gpuData == nullptr)
	{
		CreateResource(sizeof(T), &datas);
	}

	unsigned int index = datas.releasedIndices.back();
	datas.releasedIndices.pop_back();

	result = index;
	datas.refCount[result] ++;
	return result;
}

template<typename T>
inline void DX12GraphicResourceManager::ReleaseData(unsigned int index)
{
	GraphicData& datas = m_datas[typeid(T).hash_code()];
	
	datas.refCount[index] --;
	if (datas.refCount[index] <= 0)
	{
		datas.refCount[index] = 0;

		datas.releasedIndices.push_back(index);
	}
}

template<typename T>
inline void DX12GraphicResourceManager::AddRef(unsigned int index)
{
	GraphicData& datas = m_datas[typeid(T).hash_code()];

	datas.refCount[index]++;
}

template<typename T>
inline void DX12GraphicResourceManager::SetData(unsigned int index, const T* data)
{
	GraphicData& datas = m_datas[typeid(T).hash_code()];

	SetElementData(index, sizeof(T), data, &datas);
}

template<typename T>
inline D3D12_GPU_VIRTUAL_ADDRESS DX12GraphicResourceManager::GetGpuStartAddress(unsigned int currFrame)
{
	D3D12_GPU_VIRTUAL_ADDRESS result = 0;
	GraphicData& datas = m_datas[typeid(T).hash_code()];
	return datas.gpuData->GetGPUVirtualAddress() + (currFrame * baseNumData * datas.stride);
}

template<typename T>
inline D3D12_GPU_VIRTUAL_ADDRESS DX12GraphicResourceManager::GetGpuAddress(size_t index, unsigned int currFrame)
{
	D3D12_GPU_VIRTUAL_ADDRESS result = 0;
	GraphicData& datas = m_datas[typeid(T).hash_code()];

	result = datas.gpuData->GetGPUVirtualAddress() + (index * datas.stride) + (currFrame * baseNumData * datas.stride);

	return result;
}
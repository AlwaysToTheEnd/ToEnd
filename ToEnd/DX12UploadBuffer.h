#pragma once
#define WIN32_LEAN_AND_MEAN         
#include <windows.h>
#include <d3d12.h>
#include <wrl.h>
#include <memory>
#include "../Common/Source/d3dx12.h"
#include "../Common/Source/DxException.h"

template <typename T>
class DX12UploadBuffer
{
public:
	DX12UploadBuffer(ID3D12Device* device, unsigned int elementCount, bool isConstantBuffer)
	{
		m_ElementByteSize = sizeof(T);
		m_IsConstantBuffer = isConstantBuffer;
		m_NumElement = elementCount;

		if (isConstantBuffer)
		{
			m_ElementByteSize = (sizeof(T) + 255) & ~255;
		}

		auto hr = device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(static_cast<UINT64>(m_ElementByteSize) * elementCount),
			D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
			IID_PPV_ARGS(m_UploadBuffer.GetAddressOf()));

		if (FAILED(hr))
		{
			ThrowIfFailed(device->GetDeviceRemovedReason());
		}

		ThrowIfFailed(m_UploadBuffer->Map(0, nullptr, reinterpret_cast<void**>(&m_MappedData)));
	}

	DX12UploadBuffer(const DX12UploadBuffer& rhs) = delete;
	DX12UploadBuffer& operator=(const DX12UploadBuffer& rhs) = delete;

	~DX12UploadBuffer()
	{
		if (m_UploadBuffer)
		{
			m_UploadBuffer->Unmap(0, nullptr);
		}

		m_MappedData = nullptr;
	}

	ID3D12Resource* Resource()const
	{
		return m_UploadBuffer.Get();
	}

	void CopyData(int elementIndex, const T& data)
	{
		std::memcpy(&m_MappedData[elementIndex * m_ElementByteSize], &data, sizeof(T));
	}

	void CopyData(int elementIndex, const T* data)
	{
		std::memcpy(&m_MappedData[elementIndex * m_ElementByteSize], data, sizeof(T));
	}

	void CopyData(int numElement, int offsetIndex, const T* data)
	{
		assert(!m_IsConstantBuffer);
		std::memcpy(&m_MappedData[offsetIndex * m_ElementByteSize], data, sizeof(T) * numElement);
	}

	T* GetMappedData() { return reinterpret_cast<T*>(m_MappedData); }
	unsigned int		GetElementByteSize() const { return m_ElementByteSize; }
	unsigned long long	GetBufferSize() const { return static_cast<unsigned long long>(m_ElementByteSize) * m_NumElement; }
	unsigned int		GetNumElement() const { return m_NumElement; }

private:
	Microsoft::WRL::ComPtr<ID3D12Resource>	m_UploadBuffer;
	unsigned char* m_MappedData = nullptr;

	unsigned int							m_NumElement = 0;
	unsigned int							m_ElementByteSize = 0;
	bool									m_IsConstantBuffer = false;
};
#include "DX12GarbageFrameResourceMG.h"
#include "GraphicDeivceDX12.h"

DX12GarbageFrameResourceMG DX12GarbageFrameResourceMG::s_instance;

void DX12GarbageFrameResourceMG::Init()
{
	if (m_device == nullptr)
	{
		m_device = GraphicDeviceDX12::GetDevice();
		ThrowIfFailed(m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(m_fence.GetAddressOf())));
		m_fenceValue = 0;
	}
}

DX12GarbageFrameResourceMG::DX12GarbageFrameResourceMG()
{

}


DX12GarbageFrameResourceMG::~DX12GarbageFrameResourceMG()
{

}

void DX12GarbageFrameResourceMG::RegistGarbege(ID3D12CommandQueue* cmdQueue, Microsoft::WRL::ComPtr<ID3D12Resource> resource, 
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> alloc)
{
	m_fenceValue++;
	ThrowIfFailed(cmdQueue->Signal(m_fence.Get(), m_fenceValue));
	m_junks.push({ {resource}, alloc, m_fenceValue });
}

void DX12GarbageFrameResourceMG::RegistGarbeges(ID3D12CommandQueue* cmdQueue, 
	std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> resources, Microsoft::WRL::ComPtr<ID3D12CommandAllocator> alloc)
{
	m_fenceValue++;
	ThrowIfFailed(cmdQueue->Signal(m_fence.Get(), m_fenceValue));
	m_junks.push({ resources, alloc, m_fenceValue });
}

void DX12GarbageFrameResourceMG::TryClearJunks()
{
	while (m_junks.size())
	{
		if (m_fence->GetCompletedValue() >= m_junks.front().fenceValue)
		{
			m_junks.pop();
		}
		else
		{
			break;
		}
	}
}

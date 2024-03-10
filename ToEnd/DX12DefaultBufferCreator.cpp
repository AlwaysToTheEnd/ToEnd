#include "DX12DefaultBufferCreator.h"
#include "GraphicDeivceDX12.h"
#include "DX12GarbageFrameResourceMG.h"

using Microsoft::WRL::ComPtr;

DX12DefaultBufferCreator DX12DefaultBufferCreator::instance;

DX12DefaultBufferCreator::DX12DefaultBufferCreator()
{
	
}

DX12DefaultBufferCreator::~DX12DefaultBufferCreator()
{
}

void DX12DefaultBufferCreator::Init()
{
	m_device = GraphicDeviceDX12::GetDevice();
	ComPtr<ID3D12CommandAllocator> cmdAlloc;

	ThrowIfFailed(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(cmdAlloc.GetAddressOf())));
	ThrowIfFailed(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, cmdAlloc.Get(), nullptr, IID_PPV_ARGS(m_cmd.GetAddressOf())));
	ThrowIfFailed(m_cmd->Close());
}

void DX12DefaultBufferCreator::CreateDefaultBuffer(const void* data, const D3D12_RESOURCE_DESC& desc, D3D12_RESOURCE_STATES endState, ID3D12Resource** out)
{
	ID3D12CommandQueue* cmdQueue = GraphicDeviceDX12::GetGraphic()->GetCommandQueue();

	ComPtr<ID3D12Resource> uplaodBuffer;
	ComPtr<ID3D12CommandAllocator> cmdAlloc;

	D3D12_HEAP_PROPERTIES heapProperties = {};
	D3D12_HEAP_PROPERTIES upProperties = {};
	D3D12_RESOURCE_BARRIER defaultBarrier;
	D3D12_RESOURCE_BARRIER endBarrier;

	ThrowIfFailed(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(cmdAlloc.GetAddressOf())));
	ThrowIfFailed(m_cmd->Reset(cmdAlloc.Get(), nullptr));

	heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
	heapProperties.CreationNodeMask = 0;
	heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

	upProperties = heapProperties;
	upProperties.Type = D3D12_HEAP_TYPE_UPLOAD;

	defaultBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	defaultBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	defaultBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
	defaultBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
	defaultBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	defaultBarrier.Transition.pResource = *out;
	endBarrier = defaultBarrier;
	endBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
	endBarrier.Transition.StateAfter = endState;

	ThrowIfFailed(m_device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &desc,
		D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(out)));
	ThrowIfFailed(m_device->CreateCommittedResource(&upProperties, D3D12_HEAP_FLAG_NONE, &desc,
		D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(uplaodBuffer.GetAddressOf())));

	{
		m_cmd->ResourceBarrier(1, &defaultBarrier);

		D3D12_SUBRESOURCE_DATA subResourceData = {};
		subResourceData.pData = data;
		subResourceData.RowPitch = desc.Width;
		subResourceData.SlicePitch = subResourceData.RowPitch;

		UpdateSubresources<1>(m_cmd.Get(), *out, uplaodBuffer.Get(), 0, 0, 1, &subResourceData);

		m_cmd->ResourceBarrier(1, &endBarrier);
	}

	ThrowIfFailed(m_cmd->Close());
	ID3D12CommandList* cmdLists[] = { m_cmd.Get() };
	cmdQueue->ExecuteCommandLists(1, cmdLists);

	DX12GarbageFrameResourceMG::s_instance.RegistGarbege(cmdQueue, uplaodBuffer, cmdAlloc);
}

void DX12DefaultBufferCreator::CreateDefaultBuffers(const std::vector<const void*>& datas, const std::vector<D3D12_RESOURCE_DESC>& descs,
	const std::vector<D3D12_RESOURCE_STATES>& endStates, std::vector<ID3D12Resource**>& outs)
{
	const size_t numResource = datas.size();
	ID3D12CommandQueue* cmdQueue = GraphicDeviceDX12::GetGraphic()->GetCommandQueue();

	std::vector<ComPtr<ID3D12Resource>> uplaodBuffers(numResource);
	std::vector<D3D12_RESOURCE_BARRIER> defaultBarriers(numResource);
	std::vector<D3D12_RESOURCE_BARRIER> endBarriers(numResource);
	ComPtr<ID3D12CommandAllocator> cmdAlloc;

	ThrowIfFailed(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(cmdAlloc.GetAddressOf())));
	ThrowIfFailed(m_cmd->Reset(cmdAlloc.Get(), nullptr));

	D3D12_HEAP_PROPERTIES heapProperties = {};
	D3D12_HEAP_PROPERTIES upProperties = {};

	heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
	heapProperties.CreationNodeMask = 0;
	heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

	upProperties = heapProperties;
	upProperties.Type = D3D12_HEAP_TYPE_UPLOAD;

	for (size_t i = 0; i < numResource; i++)
	{
		ThrowIfFailed(m_device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &descs[i],
			D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS((outs[i]))));
		ThrowIfFailed(m_device->CreateCommittedResource(&upProperties, D3D12_HEAP_FLAG_NONE, &descs[i],
			D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(uplaodBuffers[i].GetAddressOf())));

		defaultBarriers[i].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		defaultBarriers[i].Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		defaultBarriers[i].Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
		defaultBarriers[i].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
		defaultBarriers[i].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		defaultBarriers[i].Transition.pResource = *(outs[i]);

		endBarriers[i] = defaultBarriers[i];
		endBarriers[i].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
		endBarriers[i].Transition.StateAfter = endStates[i];
	}

	m_cmd->ResourceBarrier(numResource, defaultBarriers.data());
	for (size_t i = 0; i < numResource; i++)
	{
		D3D12_SUBRESOURCE_DATA subResourceData = {};
		subResourceData.pData = datas[i];
		subResourceData.RowPitch = descs[i].Width;
		subResourceData.SlicePitch = subResourceData.RowPitch;

		UpdateSubresources<1>(m_cmd.Get(), *(outs[i]), uplaodBuffers[i].Get(), 0, 0, 1, &subResourceData);
	}
	m_cmd->ResourceBarrier(numResource, endBarriers.data());

	ThrowIfFailed(m_cmd->Close());
	ID3D12CommandList* cmdLists[] = { m_cmd.Get() };
	cmdQueue->ExecuteCommandLists(1, cmdLists);

	DX12GarbageFrameResourceMG::s_instance.RegistGarbeges(cmdQueue, uplaodBuffers, cmdAlloc);
}

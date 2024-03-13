#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <vector>
#include <queue>

class DX12GarbageFrameResourceMG
{
	struct JunkResource
	{
		std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> uplaodBuffers;
		Microsoft::WRL::ComPtr<ID3D12CommandAllocator> cmdAlloc;
		UINT64 fenceValue = 0;
	};

public:
	static DX12GarbageFrameResourceMG s_instance;

	void Init();
	void TryClearJunks();
	void RegistGarbege(ID3D12CommandQueue* cmdQueue, Microsoft::WRL::ComPtr<ID3D12Resource> resource, Microsoft::WRL::ComPtr<ID3D12CommandAllocator> alloc);
	void RegistGarbeges(ID3D12CommandQueue* cmdQueue, std::vector< Microsoft::WRL::ComPtr<ID3D12Resource>>& resources, Microsoft::WRL::ComPtr<ID3D12CommandAllocator> alloc);

	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> RentCommandAllocator();

private:
	DX12GarbageFrameResourceMG();
	~DX12GarbageFrameResourceMG();

private:
	std::queue<JunkResource> m_junks;
	std::vector<Microsoft::WRL::ComPtr<ID3D12CommandAllocator>> m_allocators;

	ID3D12Device* m_device = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Fence> m_fence;
	UINT64 m_fenceValue = 0;
};


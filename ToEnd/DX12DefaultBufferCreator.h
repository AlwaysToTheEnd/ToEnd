#pragma once
#include <vector>
#include <queue>
#include <d3d12.h>
#include <wrl.h>

class DX12DefaultBufferCreator
{
	struct JunkUploadBuffers
	{
		std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> uplaodBuffers;
		Microsoft::WRL::ComPtr<ID3D12CommandAllocator> cmdAlloc;
		UINT64 fenceValue = 0;
	};

	DX12DefaultBufferCreator();
	~DX12DefaultBufferCreator();
public:
	static DX12DefaultBufferCreator instance;

	void Init();
	void CreateDefaultBuffer(const void* data, const D3D12_RESOURCE_DESC& desc,  D3D12_RESOURCE_STATES endState, ID3D12Resource** out);
	void CreateDefaultBuffers(const std::vector<const void*>& datas, const std::vector<D3D12_RESOURCE_DESC>& descs, 
		const std::vector<D3D12_RESOURCE_STATES>& endStates, std::vector<ID3D12Resource**>& outs);

	void TryClearJunkUploadBuffers();

private:
	std::queue<JunkUploadBuffers> m_junkUploadBuffers;

	ID3D12Device* m_device = nullptr;
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> m_cmd;
	Microsoft::WRL::ComPtr<ID3D12Fence> m_fence;
	UINT64 m_fenceValue = 0;

};
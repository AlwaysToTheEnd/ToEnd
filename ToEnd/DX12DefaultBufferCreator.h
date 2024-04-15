#pragma once
#include <vector>
#include <queue>
#include <d3d12.h>
#include <wrl.h>

class DX12DefaultBufferCreator
{
	DX12DefaultBufferCreator();
	~DX12DefaultBufferCreator();
public:
	static DX12DefaultBufferCreator s_instance;

	void Init();
	void CreateDefaultBuffer(const void* data, const D3D12_RESOURCE_DESC& desc,  D3D12_RESOURCE_STATES endState, ID3D12Resource** out);
	void CreateDefaultBuffers(const std::vector<const void*>& datas, const std::vector<D3D12_RESOURCE_DESC>& descs, 
		const std::vector<D3D12_RESOURCE_STATES>& endStates, std::vector<ID3D12Resource**>& outs);

private:

	ID3D12Device* m_device = nullptr;
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> m_cmd;
};
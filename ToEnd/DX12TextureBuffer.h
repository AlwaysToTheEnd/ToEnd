#pragma once
#pragma once
#include <wrl.h>
#include <d3d12.h>
#include <functional>
#include <unordered_map>
#include <string>

namespace DirectX
{
	struct TexMetadata;
	class ScratchImage;
}

struct TextureInfo;

class DX12TextureBuffer
{
private:
	enum DX12TEXTURE_VIEW_DESC_TYPE
	{
		DX12TEXTURE_VIEW_DESC_TYPE_SRV,
		DX12TEXTURE_VIEW_DESC_TYPE_UAV
	};

	struct TEXTURE
	{
		unsigned int refCount = 0;
		std::string filePath;
		Microsoft::WRL::ComPtr<ID3D12Resource> tex;
	};

	enum TEXTURE_FILE_TYPE
	{
		DDS_TEXTURE,
		WIC_TEXTURE,
		NONE_TEXTURE,
	};

	struct TextureInfoData
	{
		unsigned int type = 0;
		unsigned int mapping = 0;
		unsigned int uvIndex = 0;
		float blend = 1.0f;
		unsigned int textureOp = 0;
		unsigned int mapMode[3] = {};
	};

public:
	DX12TextureBuffer(DX12TextureBuffer& rhs) = delete;
	DX12TextureBuffer& operator=(const DX12TextureBuffer& rhs) = delete;
	DX12TextureBuffer();
	~DX12TextureBuffer();
	void Init(UINT bufferSize);

	void Open();
	void AddTexture(const TextureInfo* texInfo);
	void Close();

	D3D12_GPU_VIRTUAL_ADDRESS GetTextureInfos() const { return m_textureInfos->GetGPUVirtualAddress(); }
	UINT GetTexturesNum() const { return (UINT)m_textures.size(); }

	void CreateSRVs(D3D12_CPU_DESCRIPTOR_HANDLE srvHeapHandle);
private:
	void DataToDevice(ID3D12Device* device, const DirectX::TexMetadata& metadata, const DirectX::ScratchImage& scratch, TEXTURE& target);
	void EvictTexture(const std::string& filePath);
	TEXTURE* ResidentTexture(const std::string& filePath);

private:
	static std::unordered_map<std::string, TEXTURE>				s_textures;
	static std::unordered_map<std::string, TEXTURE>				s_evictedTextures;
	static Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>	s_commandList;
	static Microsoft::WRL::ComPtr<ID3D12CommandAllocator>		s_comAlloc;
	static Microsoft::WRL::ComPtr<ID3D12Fence>					s_fence;
	static UINT64												s_fenceCount;
	static UINT													s_srvuavDescriptorSize;

	TextureInfoData* 											m_textureInfoMapped;
	Microsoft::WRL::ComPtr<ID3D12Resource>						m_textureInfos;
	std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>>			m_uploadBuffers;
	std::vector<TEXTURE*>										m_textures;
	bool														m_isClosed;
};
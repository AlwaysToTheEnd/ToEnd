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

class DX12TextureBuffer
{
private:
	struct TEXTURE
	{
		unsigned int refCount = 0;
		std::string filePath;
		Microsoft::WRL::ComPtr<ID3D12Resource> tex;
	};

public:
	DX12TextureBuffer(DX12TextureBuffer& rhs) = delete;
	DX12TextureBuffer& operator=(const DX12TextureBuffer& rhs) = delete;
	DX12TextureBuffer();
	~DX12TextureBuffer();
	static void Init();

	void SetTexture(const char* texturePath, unsigned int index);
	void CreateSRVs(D3D12_CPU_DESCRIPTOR_HANDLE srvHeapHandle);

	void SetBufferSize(unsigned int size) { m_textures.resize(size); }
	UINT GetTexturesNum() const { return (UINT)m_textures.size(); }

private:
	Microsoft::WRL::ComPtr<ID3D12Resource> DataToDevice(const DirectX::TexMetadata& metadata, const DirectX::ScratchImage& scratch, TEXTURE& target);
	void EvictTexture(const std::string& filePath);
	TEXTURE* ResidentTexture(const std::string& filePath);

private:
	static ID3D12Device*										s_device;
	static std::unordered_map<std::string, TEXTURE>				s_textures;
	static std::unordered_map<std::string, TEXTURE>				s_evictedTextures;
	static Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>	s_commandList;
	static UINT													s_srvuavDescriptorSize;

	std::vector<TEXTURE*>										m_textures;
};
#include "CDT/include/CDT.h"
#include "DX12FontManager.h"
#include "CGHFontLoader.h"
#include "GraphicDeivceDX12.h"

DX12FontManager::DX12FontManager()
{
	m_fontLoader = new CGHFontLoader;
}

DX12FontManager::~DX12FontManager()
{
	if (m_fontLoader)
	{
		delete m_fontLoader;
	}
}

DX12Font* DX12FontManager::GetFont(const char* filePath)
{
	if (m_device == nullptr)
	{
		m_device = GraphicDeviceDX12::GetDevice();

		ThrowIfFailed(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(m_alloc.GetAddressOf())));
		ThrowIfFailed(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_alloc.Get(), nullptr, IID_PPV_ARGS(m_cmd.GetAddressOf())));
		ThrowIfFailed(m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(m_fence.GetAddressOf())));
	}

	DX12Font* result = nullptr;
	auto iter = m_fonts.find(filePath);

	if (iter == m_fonts.end())
	{
		result = CreateDX12Font(filePath);
	}
	else
	{
		result = &iter->second;
	}

	return result;
}

DX12Font* DX12FontManager::CreateDX12Font(const char* filePath)
{
	auto cmdQueue = GraphicDeviceDX12::GetGraphic()->GetCommandQueue();
	auto& currFontData = m_fonts[filePath];

	std::unordered_map<unsigned int, CDT::Triangulation<float>> cdts;
	std::unordered_map<unsigned int, CGHFontGlyphInfo> fontInfos;
	m_fontLoader->CreateFontData(filePath, 10, &cdts, &fontInfos);
	

}
